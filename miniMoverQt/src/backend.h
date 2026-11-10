#pragma once
#include <QObject>
#include <QThread>
#include <QVariantMap>
#include <QStringList>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(QStringList models READ models CONSTANT)
public:
    explicit Backend(QObject *parent = nullptr);
    ~Backend() override;
    QVariantMap state() const { return m_state; }
    QStringList models() const { return m_models; }
    Q_INVOKABLE void command(const QString &action, const QVariantMap &arguments = {});
signals:
    void stateChanged();
    void dispatch(const QString &action, const QVariantMap &arguments);
private:
    QThread m_thread;
    QVariantMap m_state;
    QStringList m_models;
};
