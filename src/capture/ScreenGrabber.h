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

    // Whether the mouse pointer is painted into the snapshot (where supported).
    void setIncludeCursor(bool include) { m_includeCursor = include; }
    bool includeCursor() const { return m_includeCursor; }

signals:
    void captured(const DesktopSnapshot &snapshot);
    void canceled();
    void failed(const QString &message);

private:
    bool m_includeCursor = false;
};
