#pragma once

#include "DesktopSnapshot.h"

// Paints the current mouse pointer into the snapshot's screen image, at the
// position and in the shape it has right now. Does nothing where unsupported.
void drawCursor(DesktopSnapshot &snapshot);
