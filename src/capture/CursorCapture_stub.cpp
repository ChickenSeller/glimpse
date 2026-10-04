#include "CursorCapture.h"

// TODO: X11 via XFixesGetCursorImage. The Wayland screenshot portal has no
// option to include the pointer.
void drawCursor(DesktopSnapshot &)
{
}

void drawCursor(QImage &, QScreen *, const QPoint &)
{
}
