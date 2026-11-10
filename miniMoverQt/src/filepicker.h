#pragma once
#include <QObject>
#include <QVariantMap>

class FilePicker : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE virtual void pick(const QString &purpose, bool save = false) = 0;
signals:
    void chosen(const QString &purpose, const QString &path);
    void error(const QString &message);
};

class PortalFilePicker : public FilePicker {
    Q_OBJECT
public:
    using FilePicker::FilePicker;
    void pick(const QString &purpose, bool save = false) override;
private slots:
    void response(uint code, const QVariantMap &results);
private:
    QString m_request, m_purpose;
};
