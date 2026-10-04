#pragma once

#include <QObject>
#include <QString>

// Global mouse-button and keyboard events, for showing clicks and keys in a
// recording. Only reports while started; never consumes the events. Where
// the platform offers no global input (Wayland, X11 for now) isSupported()
// is false and nothing is reported.
class InputMonitor : public QObject
{
    Q_OBJECT

public:
    static InputMonitor *create(QObject *parent = nullptr);
    ~InputMonitor() override = default;

    virtual bool isSupported() const = 0;
    virtual void start() = 0;
    virtual void stop() = 0;

signals:
    void mouseButton(Qt::MouseButton button, bool pressed);
    // A key with the modifiers held, e.g. "Ctrl + Shift + S"; plain typing
    // reports single keys ("A", "Space").
    void keyPressed(const QString &combination);
    // Every key going down or up, by Windows virtual-key code (left/right
    // modifiers distinguished); auto-repeat reports "pressed" again.
    void keyState(int virtualKey, bool pressed);

protected:
    using QObject::QObject;
};
