#pragma once

#include "ScreenGrabber.h"

#include <QVariantMap>

// Wayland backend: org.freedesktop.portal.Screenshot via xdg-desktop-portal.
class PortalScreenGrabber : public ScreenGrabber
{
    Q_OBJECT

public:
    using ScreenGrabber::ScreenGrabber;

    void grab() override;
    void resetPermission() override;
    bool needsPermission() const override { return true; }

private slots:
    void onResponse(uint response, const QVariantMap &results);

private:
    void watchRequest(const QString &path);
    void unwatchRequest();

    QString m_requestPath;
};
