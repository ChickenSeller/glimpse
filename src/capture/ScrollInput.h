#pragma once

// Input injection for scrolling capture. Implemented on Windows only so far
// (Platform::supportsScrollingCapture()); elsewhere these do nothing.
namespace ScrollInput {

// Turns the mouse wheel under the pointer; negative notches scroll down.
void wheel(int notches);

// Whether Esc is down or was pressed since the last call, whichever window
// has the focus (the scrolled application usually does).
bool escapePressed();

// Watches for the user turning the wheel themselves (not wheel()) while on.
void watchUser(bool on);
// Whether the user turned the wheel since the last call while watching.
bool userScrolled();

// Whether the window under the pointer runs with higher rights than Glimpse
// (as administrator), so that Windows drops the wheel input sent to it.
bool targetOutOfReach();

} // namespace ScrollInput
