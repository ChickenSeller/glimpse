#pragma once

#include "KeyboardOverlay.h"

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
    // Every key going down or up, by where it is on the keyboard (the ids of
    // KeyboardOverlay); auto-repeat reports "pressed" again.
    void keyState(int key, bool pressed);

protected:
    using QObject::QObject;
};

// The keyboard layout of the window being typed into, and the arrangement
// of keys that goes with it. Cheap to call again: it is read once per layout.
KeyboardLayout currentKeyboardLayout();
// The same for a given layout ("0000040C": French), loaded without making it
// the one in use; for tests. The US one where there is no such layout.
KeyboardLayout keyboardLayoutNamed(const QString &name);
