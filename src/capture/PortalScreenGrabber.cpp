#include "PortalScreenGrabber.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <QScreen>
#include <QUrl>

namespace {

const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kObjectPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kRequestInterface = QStringLiteral("org.freedesktop.portal.Request");

} // namespace

void PortalScreenGrabber::grab()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        emit failed(tr("The session D-Bus is not available."));
        return;
    }

    // Subscribe to the Response signal before calling, using the request path the
    // portal will derive from our unique name and token, so we cannot miss it.
    const QString token = QStringLiteral("glimpse%1").arg(QRandomGenerator::global()->generate());
    const QString sender = bus.baseService().mid(1).replace(QLatin1Char('.'), QLatin1Char('_'));
    watchRequest(QStringLiteral("%1/request/%2/%3").arg(kObjectPath, sender, token));

    QDBusMessage message = QDBusMessage::createMethodCall(
        kService, kObjectPath, QStringLiteral("org.freedesktop.portal.Screenshot"),
        QStringLiteral("Screenshot"));
    const QVariantMap options{
        {QStringLiteral("handle_token"), token},
        {QStringLiteral("interactive"), false},
        {QStringLiteral("modal"), true},
    };
    message << QString() << options;

    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        const QDBusPendingReply<QDBusObjectPath> reply = *call;
        if (reply.isError()) {
            unwatchRequest();
            emit failed(tr("Screenshot portal error: %1").arg(reply.error().message()));
            return;
        }
        // Very old portals ignore handle_token and pick their own path.
        const QString path = reply.value().path();
        if (path != m_requestPath) {
            unwatchRequest();
            watchRequest(path);
        }
    });
}

void PortalScreenGrabber::onResponse(uint response, const QVariantMap &results)
{
    unwatchRequest();

    if (response == 1) {
        emit canceled();
        return;
    }
    if (response != 0) {
        emit failed(tr("The screenshot portal refused the request."));
        return;
    }

    const QString path = QUrl(results.value(QStringLiteral("uri")).toString()).toLocalFile();
    const QImage image(path);
    // Some portals (GNOME) save the file into ~/Pictures; it is only a transfer file for us.
    QFile::remove(path);
    if (image.isNull()) {
        emit failed(tr("Could not load the screenshot returned by the portal."));
        return;
    }

    // The portal returns one image covering the whole desktop; split it per screen.
    const auto screens = QGuiApplication::screens();
    QRect desktop;
    for (QScreen *screen : screens)
        desktop |= screen->geometry();
    if (desktop.isEmpty()) {
        emit failed(tr("No screens are available."));
        return;
    }

    const qreal scale = image.width() / qreal(desktop.width());
    DesktopSnapshot snapshot;
    for (QScreen *screen : screens) {
        const QRect g = screen->geometry();
        const QRect source = QRectF((g.x() - desktop.x()) * scale, (g.y() - desktop.y()) * scale,
                                    g.width() * scale, g.height() * scale)
                                 .toAlignedRect()
                                 .intersected(image.rect());
        ScreenImage shot;
        shot.name = screen->name();
        shot.geometry = g;
        shot.image = image.copy(source);
        shot.image.setDevicePixelRatio(scale);
        snapshot.screens.append(shot);
    }
    emit captured(snapshot);
}

void PortalScreenGrabber::watchRequest(const QString &path)
{
    m_requestPath = path;
    QDBusConnection::sessionBus().connect(kService, m_requestPath, kRequestInterface,
                                          QStringLiteral("Response"), this,
                                          SLOT(onResponse(uint,QVariantMap)));
}

void PortalScreenGrabber::unwatchRequest()
{
    if (m_requestPath.isEmpty())
        return;
    QDBusConnection::sessionBus().disconnect(kService, m_requestPath, kRequestInterface,
                                             QStringLiteral("Response"), this,
                                             SLOT(onResponse(uint,QVariantMap)));
    m_requestPath.clear();
}
