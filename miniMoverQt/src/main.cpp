#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTextStream>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include "backend.h"
#include "filepicker.h"
#include "theme.h"
#include "translator.h"
#include "worker.h"
#include "debug.h"

int main(int argc, char **argv) {
    const QString action = argc > 1 ? QString::fromLocal8Bit(argv[1]) : "open";
    const bool headless = action != "open";
    if (headless) qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    app.setApplicationName("minimover-qt");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("minimover-qt");
    if (headless) {
        const QStringList args = app.arguments();
        const bool json = args.contains("--json");
        if (json) debugSetConsoleLevel(DBG_NONE);
        auto report = [json](bool ok, const QString &message) {
            const QString output = json ? QString::fromUtf8(QJsonDocument(QJsonObject{{"ok", ok}, {"message", message}}).toJson(QJsonDocument::Compact)) : message;
            QTextStream(ok ? stdout : stderr) << output << Qt::endl;
        };
        if (action == "models") {
            QJsonObject models;
            for (int i = 0; i < XYZV3::getInfoCount(); ++i) models.insert(QString::number(i), QString::fromUtf8(XYZV3::indexToInfo(i)->screenName));
            QTextStream(stdout) << QJsonDocument(models).toJson(json ? QJsonDocument::Compact : QJsonDocument::Indented);
            return 0;
        }
        if (action == "skill" && args.size() >= 4 && args.at(2) == "install") {
            const QString directory = args.at(3);
            QFile source(":/skill.md");
            const QString path = QDir(directory).filePath("SKILL.md");
            QSaveFile output(path);
            if (QFileInfo::exists(path) || !QDir().mkpath(directory) ||
                !source.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
                report(false, "Cannot install skill. Choose a writable directory without an existing SKILL.md.");
                return 1;
            }
            auto data = source.readAll();
            const bool ok = output.write(data) == data.size() && output.commit();
            report(ok, ok ? path : "Failed to write skill.");
            return ok ? 0 : 1;
        }
        if (action == "convert" && args.size() >= 4) {
            bool modelOk = false;
            int model = -1;
            int option = args.indexOf("--model");
            if (option >= 0 && option + 1 < args.size()) model = args.at(option + 1).toInt(&modelOk);
            if (option >= 0 && !modelOk) { report(false, "Invalid --model index."); return 1; }
            Worker worker;
            bool started = false;
            int result = 1;
            QObject::connect(&worker, &Worker::updated, &app, [&](const QVariantMap &state) {
                started = started || state.value("busy").toBool();
                if (!state.value("busy").toBool()) {
                    const bool ok = started && !state.value("error").toBool();
                    result = ok ? 0 : 1;
                    report(ok, state.value("message").toString());
                    QTimer::singleShot(0, &app, [&app] { app.quit(); });
                }
            });
            worker.initialize();
            worker.command("convert", {{"input", args.at(2)}, {"output", args.at(3)}, {"model", model}});
            app.exec();
            return result;
        }
        report(false, "Usage: minimover-qt [open] | models [--json] | convert INPUT OUTPUT [--model INDEX] [--json] | skill install DIRECTORY [--json]");
        return 1;
    }
    QQuickStyle::setStyle("Material");
    app.setWindowIcon(QIcon(":/minimover-qt.svg"));
    Translator translator;
    app.installTranslator(&translator);
    Theme theme;
    Backend backend;
    PortalFilePicker picker;
    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("filePicker", &picker);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
