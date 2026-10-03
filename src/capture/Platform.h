#pragma once

#include <QString>

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

// Short explanation shown on disabled controls.
QString unsupportedHint();

} // namespace Platform
