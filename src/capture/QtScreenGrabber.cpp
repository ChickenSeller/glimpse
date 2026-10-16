#include "QtScreenGrabber.h"

#include "CursorCapture.h"
#include "WindowList.h"

#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>

#include <cmath>

void QtScreenGrabber::grab()
{
    DesktopSnapshot snapshot;
    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        const QPixmap pixmap = screen->grabWindow(0);
        if (pixmap.isNull())
            continue;

        ScreenImage shot;
        shot.name = screen->name();
        shot.geometry = screen->geometry();
        shot.image = pixmap.toImage();
        // The screen's own ratio, which windows are placed with. Derived from
        // the pixel count instead where the two disagree: at 150% a 2560 px
        // screen is 1706.67 logical px wide, reported as 1707, and a ratio
        // derived from that would put crops a pixel off.
        const qreal stated = screen->devicePixelRatio();
        const qreal derived = shot.image.width() / qreal(shot.geometry.width());
        const bool fits = std::abs(shot.image.width() - shot.geometry.width() * stated) <= 1.0;
        shot.image.setDevicePixelRatio(fits ? stated : derived);
        snapshot.screens.append(shot);
    }

    if (includeCursor())
        drawCursor(snapshot);

    // Window positions are frozen together with the pixels; once the selection
    // overlay is up it would cover everything a live query could find.
    snapshot.windows = enumerateWindows();

    if (snapshot.isEmpty()) {
        emit failed(tr("Could not read the screen contents."));
        return;
    }
    emit captured(snapshot);
}
