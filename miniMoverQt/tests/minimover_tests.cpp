#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickStyle>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include "backend.h"
#include "worker.h"
#include "theme.h"
#include "translator.h"
#include "filepicker.h"
#include "aes.h"

static bool partialSend = false;
static bool interruptSend = false;
extern "C" ssize_t __real_send(int, const void *, size_t, int);
extern "C" ssize_t __wrap_send(int fd, const void *buf, size_t size, int flags) {
    if (interruptSend) { interruptSend = false; errno = EINTR; return -1; }
    return __real_send(fd, buf, partialSend ? qMin(size, size_t(3)) : size, flags);
}

class TestProtocol : public XYZV3 {
public:
    using XYZV3::pkcs7unpad;
    using XYZV3::processGCode;
    using XYZV3::encryptFile;
    using XYZV3::decryptFile;
    using XYZV3::encryptHeader;
    using XYZV3::encryptBody;
    using XYZV3::parseStatusSubstring;
    using XYZV3::sendFileInit;
    using XYZV3::sendFileProcess;
    using XYZV3::sendFileFinalize;
    using XYZV3::m_status;
    using XYZV3::m_info;
    int lastBlockSize() const { return pDat.lastBlockSize; }
};

class MemoryStream : public StreamT {
public:
    QByteArray data;
    bool open = true;
    bool wifi = false;
    QList<QByteArray> writes;
    int reads = 0;
    bool reopenStream() override { open = true; return true; }
    void closeStream() override { open = false; }
    bool isOpen() override { return open; }
    void clear() override { StreamT::clear(); }
    int read(char *buf, int size, bool terminate = true) override {
        ++reads;
        int count = qMin(int(data.size()), size - (terminate ? 1 : 0));
        memcpy(buf, data.constData(), count);
        data.remove(0, count);
        if (terminate) buf[count] = 0;
        return count;
    }
    int write(const char *bytes, int size) override { writes.append(QByteArray(bytes, size)); return size; }
    bool isWIFI() override { return wifi; }
    float getDefaultTimeout() override { return 1; }
    void seed(const QByteArray &bytes) {
        Q_ASSERT(bytes.size() < m_lineBufLen);
        memcpy(m_lineBuf, bytes.constData(), bytes.size());
        m_lineBufCount = bytes.size();
        m_lineBuf[m_lineBufCount] = 0;
    }
};

class TestSocket : public Socket {
public:
    void adopt(int fd) { m_soc = fd; }
};

class FakePicker : public FilePicker {
    Q_OBJECT
public:
    using FilePicker::FilePicker;
    void pick(const QString &purpose, bool) override { emit chosen(purpose, "/example/cube.gcode"); }
};

class MiniMoverTests : public QObject {
    Q_OBJECT
private:
    static bool write(const QString &path, const QByteArray &data) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    }
    static QByteArray read(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {};
        return file.readAll();
    }
    static QVariantMap last(const QSignalSpy &spy) { return spy.last().at(0).toMap(); }
private slots:
    void initTestCase() { QQuickStyle::setStyle("Material"); }

    void lineBufferPreservesBytes() {
        MemoryStream stream;
        char out[4];
        stream.seed("abcdef\nnext\n");
        QCOMPARE(stream.readLineFromBuffer(out, sizeof(out), false), 3);
        QCOMPARE(QByteArray(out), QByteArray("abc"));
        QCOMPARE(stream.readLineFromBuffer(out, sizeof(out), false), 3);
        QCOMPARE(QByteArray(out), QByteArray("def"));
        QCOMPARE(stream.readLineFromBuffer(out, sizeof(out), false), 3);
        QCOMPARE(QByteArray(out), QByteArray("nex"));
        QCOMPARE(stream.readLineFromBuffer(out, sizeof(out), false), 1);
        QCOMPARE(QByteArray(out), QByteArray("t"));
        QCOMPARE(stream.m_lineBufCount, 0);
    }

    void emptyAndPartialLines() {
        MemoryStream stream;
        char out[16];
        stream.seed("\nhello\n");
        QCOMPARE(stream.readLineFromBuffer(out, sizeof(out), false), 0);
        QCOMPARE(stream.m_lineBufCount, 6);
        QCOMPARE(stream.readLineFromBuffer(out, sizeof(out), false), 5);
        stream.seed("partial");
        QCOMPARE(stream.readLineFromBuffer(out, 4, true), 3);
        QCOMPARE(QByteArray(out), QByteArray("par"));
        QCOMPARE(stream.readLineFromBuffer(out, sizeof(out), true), 4);
        QCOMPARE(QByteArray(out), QByteArray("tial"));
        QCOMPARE(stream.m_lineBufCount, 0);
    }

    void fullLineBufferDoesNotStall() {
        MemoryStream stream;
        char out[4096];
        stream.data = QByteArray(4095, 'a');
        QCOMPARE(stream.readLine(out, sizeof(out)), 0);
        QCOMPARE(stream.readLine(out, sizeof(out)), 4095);
        QCOMPARE(strlen(out), size_t(4095));
        QCOMPARE(stream.reads, 1);
        QCOMPARE(stream.m_lineBufCount, 0);
    }

    void socketPartialWritesAndBinaryLogging() {
        int pair[2];
        QCOMPARE(::socketpair(AF_UNIX, SOCK_STREAM, 0, pair), 0);
        TestSocket socket;
        socket.adopt(pair[0]);
        const QByteArray data = QByteArray::fromHex("0102030405060708090aff");
        partialSend = true;
        interruptSend = true;
        QCOMPARE(socket.write(data.constData(), data.size()), data.size());
        partialSend = false;
        char out[32];
        const auto count = ::read(pair[1], out, sizeof(out));
        QCOMPARE(QByteArray(out, count), data);
        ::close(pair[1]);
    }

    void socketZeroDescriptorAndClosedPeer() {
        int saved = ::dup(STDIN_FILENO);
        int pair[2];
        QCOMPARE(::socketpair(AF_UNIX, SOCK_STREAM, 0, pair), 0);
        QCOMPARE(::dup2(pair[0], STDIN_FILENO), 0);
        ::close(pair[0]);
        TestSocket socket;
        socket.adopt(0);
        const bool valid = socket.isOpen();
        ::close(pair[1]);
        const int count = socket.write("x", 1);
        socket.closeStream();
        if (saved >= 0) { ::dup2(saved, STDIN_FILENO); ::close(saved); }
        QVERIFY(valid);
        QCOMPARE(count, 0);
    }

    void serialIsRawAndCanReopen() {
        int master = ::posix_openpt(O_RDWR | O_NOCTTY);
        QVERIFY(master >= 0);
        QCOMPARE(::grantpt(master), 0);
        QCOMPARE(::unlockpt(master), 0);
        const QByteArray name = ::ptsname(master);
        Serial serial;
        QVERIFY(serial.openStream(name.constData(), 115200));
        const QByteArray bytes = QByteArray::fromHex("0d0a1181ff");
        QCOMPARE(::write(master, bytes.constData(), bytes.size()), bytes.size());
        char out[16];
        QCOMPARE(serial.read(out, sizeof(out), false), bytes.size());
        QCOMPARE(QByteArray(out, bytes.size()), bytes);
        QVERIFY(serial.reopenStream());
        serial.closeStream();
        QVERIFY(!serial.openStream(name.constData(), 12345));
        ::close(master);
    }

    void paddingIsValidated() {
        TestProtocol xyz;
        QByteArray shortPadding(1, char(16));
        QCOMPARE(xyz.pkcs7unpad(shortPadding.data(), 1), 1);
        QByteArray unsignedPadding(8, char(255));
        QCOMPARE(xyz.pkcs7unpad(unsignedPadding.data(), 8), 8);
        QByteArray mismatch("abc\x01\x02", 5);
        QCOMPARE(xyz.pkcs7unpad(mismatch.data(), mismatch.size()), 5);
        QByteArray valid("abc\x02\x02", 5);
        QCOMPARE(xyz.pkcs7unpad(valid.data(), valid.size()), 3);
        QCOMPARE(valid.at(3), '\0');
    }

    void aesMatchesKnownVectors() {
        const QByteArray plain = QByteArray::fromHex("6bc1bee22e409f96e93d7e117393172a");
        const QList<QByteArray> keys = {
            QByteArray::fromHex("2b7e151628aed2a6abf7158809cf4f3c"),
            QByteArray::fromHex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4")
        };
        const QList<QByteArray> expected = {
            QByteArray::fromHex("3ad77bb40d7a3660a89ecaf32466ef97"),
            QByteArray::fromHex("f3eed1bdb5d2a03c064b5a7e3db181f8")
        };
        for (int i = 0; i < keys.size(); ++i) {
            AES_ctx context;
            AES_init_ctx(&context, keys[i].constData());
            auto data = plain;
            AES_ECB_encrypt_buffer(&context, reinterpret_cast<uint8_t *>(data.data()), data.size());
            QCOMPARE(data, expected[i]);
            AES_ECB_decrypt_buffer(&context, reinterpret_cast<uint8_t *>(data.data()), data.size());
            QCOMPARE(data, plain);
        }
    }

    void conversionPreservesCommands() {
        TestProtocol xyz;
        QByteArray code = ";TIME:60\nG0 X1\nG01 X2\nG02 X3\n; G0 comment\ng0X4\nG1 X5 ; G0 comment\n";
        char *processed = nullptr;
        int header = 0, total = 0;
        QVERIFY(xyz.processGCode(code.constData(), code.size(), "dv1MX0A000", &processed, &header, &total));
        QByteArray result(processed, total);
        delete [] processed;
        QVERIFY(result.contains("G1 X1\n"));
        QVERIFY(result.contains("G01 X2\nG02 X3\n; G0 comment\ng1X4\nG1 X5 ; G0 comment\n"));
    }

    void encryptionAllocatesFullPaddingBlock() {
        TestProtocol xyz;
        for (int length : {15, 16, 17, 31, 32, 33}) {
            QByteArray source(length, ';');
            char *header = nullptr, *body = nullptr;
            int headerLen = 0, bodyLen = 0;
            QVERIFY(xyz.encryptHeader(source.constData(), length, false, &header, &headerLen));
            QVERIFY(xyz.encryptBody(source.constData(), length, false, &body, &bodyLen));
            QCOMPARE(headerLen, ((length / 16) + 1) * 16);
            QCOMPARE(bodyLen, headerLen);
            delete [] header;
            delete [] body;
        }
    }

    void conversionRoundTripsEveryModel() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        TestProtocol xyz;
        QByteArray code = ";TIME:60\nG0 X1\n";
        for (int i = 0; i < 1200; ++i) code += "G1 X" + QByteArray::number(i % 100) + " Y10 E1\n";
        const auto input = dir.filePath("input.gcode"), encoded = dir.filePath("print.3w"), output = dir.filePath("output.gcode");
        QVERIFY(write(input, code));
        for (int model = 0; model < XYZV3::getInfoCount(); ++model) {
            QVERIFY2(xyz.encryptFile(qPrintable(input), qPrintable(encoded), model), XYZV3::indexToInfo(model)->screenName);
            QVERIFY2(xyz.decryptFile(qPrintable(encoded), qPrintable(output)), XYZV3::indexToInfo(model)->screenName);
            QVERIFY(read(output).contains("G1 X1\n"));
            QVERIFY(read(output).endsWith("G1 X99 Y10 E1\n"));
        }
        const auto before = read(input);
        QVERIFY(!xyz.encryptFile(qPrintable(input), qPrintable(input), 0));
        QCOMPARE(read(input), before);
    }

    void malformedFilesFailWithoutCrashing() {
        QTemporaryDir dir;
        TestProtocol xyz;
        const auto input = dir.filePath("bad.3w"), output = dir.filePath("out.gcode");
        for (int length : {0, 1, 12, 19, 8192, 8193, 8201}) {
            QByteArray bytes(length, char(0xff));
            if (length >= 12) bytes.replace(0, 12, "3DPFNKG13WTW");
            QVERIFY(write(input, bytes));
            QVERIFY(!xyz.decryptFile(qPrintable(input), qPrintable(output)));
        }
    }

    void unknownModelsAndOversizedStatusAreSafe() {
        TestProtocol xyz;
        QVERIFY(!XYZV3::modelToInfo("unknown"));
        QVERIFY(!XYZV3::serialToInfo("unknown"));
        for (const QByteArray prefix : {"i:", "l:", "n:", "p:", "v:", "V:", "4:", "w:1,", "X:1,"}) {
            QByteArray data = prefix + QByteArray(1000, 'a');
            xyz.parseStatusSubstring(data.constData());
        }
        QCOMPARE(strlen(xyz.m_status.nMachineName), size_t(63));
        QCOMPARE(strlen(xyz.m_status.lLang), size_t(31));
        QVERIFY(!xyz.getPrinterInfo());
    }

    void uploadKeepsExactBlockMultiples() {
        QTemporaryDir dir;
        const QString input = dir.filePath("print.3w");
        const QByteArray data(16384, 'x');
        QVERIFY(write(input, data));
        MemoryStream stream;
        stream.wifi = true;
        TestProtocol xyz;
        xyz.setStream(&stream);
        xyz.m_status.isValid = true;
        xyz.m_status.oPacketSize = 8192;
        QVERIFY(xyz.sendFileInit(qPrintable(input), true));
        QCOMPARE(xyz.lastBlockSize(), 8192);
        stream.writes.clear();
        QVERIFY(xyz.sendFileProcess());
        QCOMPARE(stream.writes.size(), 2);
        for (const auto &packet : stream.writes) {
            QCOMPARE(packet.size(), 8204);
            QCOMPARE(packet.mid(8, 8192), data.left(8192));
        }
        QVERIFY(!xyz.sendFileProcess());
        QVERIFY(xyz.sendFileFinalize());
        xyz.setStream(nullptr);
    }

    void failedGcodeConversionNeverUploadsRawInput() {
        MemoryStream stream;
        TestProtocol xyz;
        xyz.setStream(&stream);
        xyz.m_info = XYZV3::indexToInfo(12);
        xyz.printFileStart("/nonexistent.gcode");
        xyz.doProcess();
        QVERIFY(!xyz.isInProgress());
        QVERIFY(!xyz.isSuccess());
        QVERIFY(stream.writes.isEmpty());
        xyz.setStream(nullptr);
    }

    void workerRejectsUnsafeActions() {
        Worker worker;
        QSignalSpy spy(&worker, &Worker::updated);
        worker.command("home");
        QVERIFY(last(spy).value("error").toBool());
        QVERIFY(!last(spy).value("busy").toBool());
        worker.command("convert", {{"input", "/nonexistent.gcode"}, {"output", "/nonexistent.3w"}});
        QVERIFY(last(spy).value("error").toBool());
    }

    void workerConvertsAtomically() {
        QTemporaryDir dir;
        const auto input = dir.filePath("input.gcode"), output = dir.filePath("output.3w");
        QVERIFY(write(input, ";TIME:60\nG1 X10\n"));
        Worker worker;
        QSignalSpy spy(&worker, &Worker::updated);
        worker.initialize();
        QVariantMap args{{"input", input}, {"output", output}, {"model", 13}};
        worker.command("convert", args);
        QVERIFY(last(spy).value("busy").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(!last(spy).value("busy").toBool(), 5000);
        QVERIFY(!last(spy).value("error").toBool());
        QVERIFY(read(output).startsWith("3DPFNKG13WTW"));
        auto before = read(output);
        worker.command("convert", args);
        QVERIFY(last(spy).value("error").toBool());
        QCOMPARE(read(output), before);
        QCOMPARE(QDir(dir.path()).entryList({".minimover-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).size(), 0);
    }

    void themeAndLanguageFollowFiles() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        const auto colors = dir.filePath("theme/colors.toml"), language = dir.filePath("language");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QVERIFY(write(colors, "accent = '#123456'\n"));
        QTRY_COMPARE(theme.accent(), QString("#123456"));
        Translator translator(language);
        QVERIFY(QCoreApplication::installTranslator(&translator));
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(write(language, "cs\n"));
        QTRY_COMPARE(translator.language(), QString("cs"));
        QCOMPARE(translator.translate("Main", "Connect"), QString::fromUtf8("Připojit"));
        QCOMPARE(QCoreApplication::translate("Main", "Connect"), QString::fromUtf8("Připojit"));
        QVERIFY(write(language, "en\n"));
        QTRY_COMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Connect").isNull());
        QCoreApplication::removeTranslator(&translator);
    }

    void realQmlLoadsAndShortcutsWork() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        Translator translator(dir.filePath("language"));
        QVERIFY(QCoreApplication::installTranslator(&translator));
        Backend backend;
        FakePicker picker;
        QQmlApplicationEngine engine;
        connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("translator", &translator);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.rootContext()->setContextProperty("filePicker", &picker);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTest::keyClick(window, Qt::Key_O, Qt::ControlModifier);
        auto *path = window->findChild<QObject *>("printPath");
        QVERIFY(path);
        QTRY_COMPARE(path->property("text").toString(), QString("/example/cube.gcode"));
        if (qEnvironmentVariableIsSet("MINIMOVER_SCREENSHOT")) {
            QTest::qWait(300);
            QVERIFY(window->grabWindow().save(qEnvironmentVariable("MINIMOVER_SCREENSHOT")));
        }
        auto *tabs = window->findChild<QObject *>("tabs");
        QVERIFY(tabs);
        tabs->setProperty("currentIndex", 1);
        auto *input = window->findChild<QObject *>("inputPath");
        auto *output = window->findChild<QObject *>("outputPath");
        auto *model = window->findChild<QObject *>("model");
        auto *convert = window->findChild<QQuickItem *>("convertButton");
        QVERIFY(input && output && model && convert);
        const auto source = dir.filePath("input.gcode"), destination = dir.filePath("output.3w");
        QVERIFY(write(source, ";TIME:60\nG1 X1\n"));
        input->setProperty("text", source);
        output->setProperty("text", destination);
        model->setProperty("currentIndex", 12);
        QTRY_VERIFY(convert->isVisible() && convert->isEnabled());
        QTest::qWait(100);
        const auto center = convert->mapToScene(QPointF(convert->width() / 2, convert->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(destination) && !backend.state().value("busy").toBool(), 5000);
        QVERIFY(!backend.state().value("error").toBool());
        QVERIFY(write(dir.filePath("language"), "da\n"));
        QTRY_COMPARE(convert->property("text").toString(), QString("Konverter"));
        if (qEnvironmentVariableIsSet("MINIMOVER_SCREENSHOT_DA")) {
            QTest::qWait(300);
            QVERIFY(window->grabWindow().save(qEnvironmentVariable("MINIMOVER_SCREENSHOT_DA")));
        }
        QCOMPARE(warnings.size(), 0);
        QCoreApplication::removeTranslator(&translator);
    }
};

QTEST_MAIN(MiniMoverTests)
#include "minimover_tests.moc"
