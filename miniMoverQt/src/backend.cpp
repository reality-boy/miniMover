#include "backend.h"
#include "worker.h"

Backend::Backend(QObject *parent) : QObject(parent) {
    m_state = {{"connected", false}, {"busy", false}, {"printing", false},
               {"message", tr("Not connected")}, {"progress", 0}, {"prompt", QString()}};
    for (int i = 0; i < XYZV3::getInfoCount(); ++i)
        m_models.append(QString::fromUtf8(XYZV3::indexToInfo(i)->screenName));
    auto *worker = new Worker;
    worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::started, worker, &Worker::initialize);
    connect(&m_thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &Backend::dispatch, worker, &Worker::command);
    connect(worker, &Worker::updated, this, [this](const QVariantMap &state) {
        m_state = state;
        emit stateChanged();
    });
    m_thread.start();
}

Backend::~Backend() {
    m_thread.quit();
    m_thread.wait();
}

void Backend::command(const QString &action, const QVariantMap &arguments) {
    emit dispatch(action, arguments);
}
