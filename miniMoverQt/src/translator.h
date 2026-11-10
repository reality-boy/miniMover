#pragma once
#include <QFileSystemWatcher>
#include <QHash>
#include <QTimer>
#include <QTranslator>

class Translator : public QTranslator {
    Q_OBJECT
    Q_PROPERTY(QString language READ language NOTIFY changed)
public:
    explicit Translator(const QString &languageFile = {}, QObject *parent = nullptr);
    QString language() const { return m_language; }
    // Stay installed even when English has no catalog, so live switches work.
    bool isEmpty() const override { return false; }
    QString translate(const char *, const char *source, const char * = nullptr, int = -1) const override;
signals:
    void changed();
private:
    void reload();
    QString m_file, m_language;
    QHash<QString, QString> m_strings;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
