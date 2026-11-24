#pragma once

// Input injection for scrolling capture. Implemented on Windows only so far
// (Platform::supportsScrollingCapture()); elsewhere these do nothing.
namespace ScrollInput {

// Which wheel the functions below turn and hold back: the vertical one
// (the default), or the horizontal one, for content that scrolls sideways.
void setHorizontal(bool horizontal);

// Turns the mouse wheel under the pointer; negative notches scroll down (or
// right, horizontally).
void wheel(int notches);
// As if the user turned the wheel (not marked as Glimpse's); for tests.
void userWheel(int notches);

// Whether Esc is down or was pressed since the last call, whichever window
// has the focus (the scrolled application usually does).
bool escapePressed();

// Watches for the user turning the wheel themselves (not wheel()) while on.
void watchUser(bool on);
// Whether the user turned the wheel since the last call while watching.
bool userScrolled();

// While on, the user's turns of the wheel are held back instead of reaching
// any window, and passed on by passOnPending(): scrolling capture passes
// them on step by step, never further than it can follow. Horizontally,
// that is the horizontal wheel (a touchpad's sideways swipe, a tilt wheel)
// and the vertical one turned with Shift held; the vertical wheel alone
// still scrolls the other way, unheld.
void pace(bool on);
// Passes on up to `limit` wheel units (120 a notch; parts of a notch scroll
// part of the way in most applications) of what the user turned, in their
// direction, as they turned it; returns how much, negative for down (or
// right). 0 when nothing is pending.
int passOnPending(int limit);

// Whether the window under the pointer runs with higher rights than Glimpse
// (as administrator), so that Windows drops the wheel input sent to it.
bool targetOutOfReach();

} // namespace ScrollInput
