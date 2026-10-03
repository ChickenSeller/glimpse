#pragma once

#include "DesktopSnapshot.h"

#include <vector>

// Visible top-level windows of other applications (topmost first) with their
// child controls, in logical virtual-desktop coordinates. Returns an empty list
// where the platform does not allow it (Wayland) or it is not implemented yet.
std::vector<WindowNode> enumerateWindows();
