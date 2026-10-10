#pragma once

#include <QPoint>
#include <QString>

#include <optional>

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

// Whether windows can place themselves and learn where they are. Not on
// native Wayland; through XWayland they can, though pixels still need the portal.
bool windowsCanPlaceThemselves();

// Features implemented only on some platforms so far; the UI disables the rest.
bool supportsWindowPicking(); // "Capture Window / Object" hit-testing
bool supportsCursorCapture(); // mouse pointer in captures
bool supportsGlobalHotkeys();
bool supportsScrollingCapture(); // needs wheel input injection
// Recording overlays: clicks and keys need global input events; the pointer
// highlight needs the global pointer position (not available on Wayland).
bool supportsInputOverlay();
bool supportsPointerHighlight();

// Makes a window appear and disappear instantly (no compositor fade), so a
// capture taken right after hiding it does not catch it fading out.
void disableWindowAnimations(QWidget *window);
// True where disableWindowAnimations() works, so a hidden window is gone as
// soon as the screen is next composed (see flushCompositor()).
bool hidesWindowsInstantly();
// Waits until what was just shown or hidden is on screen. Does nothing where
// unsupported.
void flushCompositor();

// The pointer position in physical pixels, where the platform reports it
// exactly. Qt's logical positions are rounded on fractional scale factors
// (e.g. 150%), which is off by a pixel for pixel-exact tools.
std::optional<QPoint> nativeCursorPos();
// Moves the pointer by whole physical pixels. False where windows may not
// move the pointer (Wayland).
bool moveCursorBy(int dx, int dy);

// Keeps a window out of screen captures and recordings (it stays visible on
// screen). Does nothing where unsupported.
void excludeFromCapture(QWidget *window);

// Short explanation shown on disabled controls.
QString unsupportedHint();

} // namespace Platform
