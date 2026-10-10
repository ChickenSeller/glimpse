#pragma once

#include "DesktopSnapshot.h"

#include <QObject>

// Takes a snapshot of every monitor. Some backends (the Wayland portal) are
// asynchronous, so the result is always delivered through a signal.
class ScreenGrabber : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // Picks the backend that works for the current display server.
    static ScreenGrabber *create(QObject *parent = nullptr);

    virtual void grab() = 0;

    // Forgets an earlier refusal so the next grab asks the user again
    // (the Wayland portal; other backends need no permission).
    virtual void resetPermission() {}

    // Whether the mouse pointer is painted into the snapshot (where supported).
    void setIncludeCursor(bool include) { m_includeCursor = include; }
    bool includeCursor() const { return m_includeCursor; }

signals:
    void captured(const DesktopSnapshot &snapshot);
    void canceled();
    void failed(const QString &message);
    // The system refused because Glimpse has no screenshot permission yet. On
    // GNOME it can only ask while a Glimpse window has the focus, so grab()
    // again from a focused window to get its permission dialog.
    void permissionNeeded();

private:
    bool m_includeCursor = false;
};
