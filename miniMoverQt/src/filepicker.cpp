#include "filepicker.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QUuid>
#include <QUrl>

void PortalFilePicker::pick(const QString &purpose, bool save) {
    if (!m_request.isEmpty()) return;
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) { emit error(tr("Desktop file picker is unavailable. Enter a path instead.")); return; }
    const QString token = "minimover_" + QUuid::createUuid().toString(QUuid::Id128);
    QString sender = bus.baseService().mid(1);
    sender.replace('.', '_');
    m_request = "/org/freedesktop/portal/desktop/request/" + sender + "/" + token;
    m_purpose = purpose;
    bus.connect("org.freedesktop.portal.Desktop", m_request, "org.freedesktop.portal.Request", "Response",
                this, SLOT(response(uint,QVariantMap)));
    auto call = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                                              "org.freedesktop.portal.FileChooser", save ? "SaveFile" : "OpenFile");
    call.setArguments({QString(), save ? tr("Choose output file") : tr("Choose print file"),
                       QVariantMap{{"handle_token", token}, {"multiple", false}}});
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, bus](QDBusPendingCallWatcher *finished) mutable {
        QDBusPendingReply<QDBusObjectPath> reply = *finished;
        finished->deleteLater();
        if (reply.isError()) {
            bus.disconnect("org.freedesktop.portal.Desktop", m_request, "org.freedesktop.portal.Request", "Response", this, nullptr);
            m_request.clear();
            emit error(tr("Desktop file picker failed. Enter a path instead."));
        } else if (!m_request.isEmpty() && reply.value().path() != m_request) {
            bus.disconnect("org.freedesktop.portal.Desktop", m_request, "org.freedesktop.portal.Request", "Response", this, nullptr);
            m_request = reply.value().path();
            bus.connect("org.freedesktop.portal.Desktop", m_request, "org.freedesktop.portal.Request", "Response",
                        this, SLOT(response(uint,QVariantMap)));
        }
    });
}

void PortalFilePicker::response(uint code, const QVariantMap &results) {
    auto bus = QDBusConnection::sessionBus();
    bus.disconnect("org.freedesktop.portal.Desktop", m_request, "org.freedesktop.portal.Request", "Response", this, nullptr);
    m_request.clear();
    if (code != 0) return;
    const auto uris = results.value("uris").toStringList();
    const QUrl url(uris.value(0));
    if (!url.isLocalFile()) { emit error(tr("Choose a local file.")); return; }
    emit chosen(m_purpose, url.toLocalFile());
}
