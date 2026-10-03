#include "QtScreenGrabber.h"

#include "CursorCapture.h"
#include "WindowList.h"

#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>

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
        // Derive the ratio from the actual pixel count rather than trusting
        // whatever the platform plugin stamped on the pixmap.
        shot.image.setDevicePixelRatio(shot.image.width() / qreal(shot.geometry.width()));
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
