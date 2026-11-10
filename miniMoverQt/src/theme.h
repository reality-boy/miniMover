#pragma once
#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
public:
    explicit Theme(const QString &directory = {}, QObject *parent = nullptr);
    ~Theme() override;
    QString accent() const { return m_accent; }
    int radius() const { return m_radius; }
signals:
    void changed();
private:
    void reload();
    QString m_directory, m_accent = "#FFD60A";
    int m_radius = 0;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce, m_poll, m_timeout;
    QProcess m_process;
};
