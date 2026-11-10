#pragma once
#include <QObject>
#include <QTimer>
#include <QTemporaryDir>
#include <QVariantMap>
#include <memory>
#include <cstdio>
#include "serial.h"
#include "socket.h"
#include "xyzv3.h"

// Only this worker thread may touch the legacy protocol library.
class Worker : public QObject {
    Q_OBJECT
public:
    explicit Worker(QObject *parent = nullptr);
public slots:
    void initialize();
    void command(const QString &action, const QVariantMap &arguments = {});
signals:
    void updated(const QVariantMap &state);
private:
    void tick();
    void publish();
    void fail(const QString &message);
    void refreshStatus();
    bool prepareConversion(const QVariantMap &arguments);
    bool commitConversion();
    Serial m_serial;
    Socket m_socket;
    XYZV3 m_xyz;
    QTimer *m_timer = nullptr;
    QVariantMap m_state;
    QString m_action, m_output, m_temporaryFile;
    std::unique_ptr<QTemporaryDir> m_temporary;
    qint64 m_lastPoll = 0;
};
