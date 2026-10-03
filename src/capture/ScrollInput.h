#pragma once

// Input injection for scrolling capture. Implemented on Windows only so far
// (Platform::supportsScrollingCapture()); elsewhere these do nothing.
namespace ScrollInput {

// Turns the mouse wheel under the pointer; negative notches scroll down.
void wheel(int notches);

// Whether Esc is down or was pressed since the last call, whichever window
// has the focus (the scrolled application usually does).
bool escapePressed();

} // namespace ScrollInput
