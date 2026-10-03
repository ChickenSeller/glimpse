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

signals:
    void captured(const DesktopSnapshot &snapshot);
    void canceled();
    void failed(const QString &message);
};
