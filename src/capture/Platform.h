#pragma once

#include <QString>

class QWidget;

namespace Platform {

enum class DisplayServer {
    Windows,
    X11,
    Wayland,
    Unknown,
};

// The display server of the user's session. This is deliberately not derived
// from QGuiApplication::platformName(): an app running through XWayland reports
// "xcb" but still cannot read other clients' pixels, so it must use the portal.
DisplayServer displayServer();

QString displayServerName();

// Features implemented only on some platforms so far; the UI disables the rest.
bool supportsWindowPicking(); // "Capture Window / Object" hit-testing
bool supportsCursorCapture(); // mouse pointer in captures
bool supportsGlobalHotkeys();
bool supportsScrollingCapture(); // needs wheel input injection

// Makes a window appear and disappear instantly (no compositor fade), so a
// capture taken right after hiding it does not catch it fading out.
void disableWindowAnimations(QWidget *window);

// Short explanation shown on disabled controls.
QString unsupportedHint();

} // namespace Platform
