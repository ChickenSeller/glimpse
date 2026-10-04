#pragma once

#include "DesktopSnapshot.h"

// Paints the current mouse pointer into the snapshot's screen image, at the
// position and in the shape it has right now. Does nothing where unsupported.
void drawCursor(DesktopSnapshot &snapshot);

class QScreen;
// Paints the current pointer into `image`, which shows part of `screen` in
// physical pixels starting at `origin` (relative to the monitor's top-left).
// Nothing happens if the pointer is on another screen.
void drawCursor(QImage &image, QScreen *screen, const QPoint &origin);
