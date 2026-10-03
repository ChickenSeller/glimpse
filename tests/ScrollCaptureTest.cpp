// glimpse-scroll-test: end-to-end check of scrolling capture on a window it
// owns. Shows a scroll area with a known tall image, runs ScrollCaptureSession
// on its viewport (real wheel input, real screen grabs) and compares the
// stitched result with the image. Moves the mouse for a few seconds.
//
//   glimpse-scroll-test [--save <png>]

#include "capture/ScrollCaptureSession.h"

#include <QApplication>
#include <QLabel>
#include <QPainter>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdlib>

namespace {

QImage makeContent(QSize size, qreal dpr)
{
    QImage image(size * dpr, QImage::Format_RGB32);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::white);
    QPainter p(&image);
    QFont font = p.font();
    font.setPixelSize(14);
    p.setFont(font);
    for (int y = 0, n = 1; y < size.height(); y += 24, ++n) {
        p.fillRect(QRect(0, y, 6, 20), QColor::fromHsv((n * 37) % 360, 160, 200));
        p.setPen(Qt::black);
        p.drawText(16, y + 16, QStringLiteral("Line %1  —  the quick brown fox jumps over the lazy dog").arg(n));
    }
    return image;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const QStringList args = QApplication::arguments();
    const qsizetype saveAt = args.indexOf(QStringLiteral("--save"));
    const QString savePath = saveAt >= 0 && saveAt + 1 < args.size() ? args.at(saveAt + 1) : QString();

    // --rect x,y,w,h: scroll-capture an arbitrary screen area (logical
    // coordinates), e.g. another application's view, and just save the result.
    // Combine with QT_LOGGING_RULES="glimpse.scroll.debug=true" for diagnostics.
    if (const qsizetype rectAt = args.indexOf(QStringLiteral("--rect")); rectAt >= 0 && rectAt + 1 < args.size()) {
        const QStringList parts = args.at(rectAt + 1).split(QLatin1Char(','));
        if (parts.size() != 4)
            return 2;
        const QRect rect(parts[0].toInt(), parts[1].toInt(), parts[2].toInt(), parts[3].toInt());
        auto *session = new ScrollCaptureSession(rect, &app);
        QObject::connect(session, &ScrollCaptureSession::finished, &app, [&](const QImage &image) {
            if (!savePath.isEmpty())
                image.save(savePath);
            std::printf("scrolling capture of %d,%d %dx%d: got %dx%d\n", rect.x(), rect.y(), rect.width(),
                        rect.height(), image.width(), image.height());
            std::fflush(stdout);
            QApplication::quit();
        });
        QTimer::singleShot(0, session, &ScrollCaptureSession::start);
        QApplication::exec();
        return 0;
    }

    const qreal dpr = QGuiApplication::primaryScreen()->devicePixelRatio();
    const QSize contentSize(560, 2400);
    const QImage content = makeContent(contentSize, dpr);

    auto *label = new QLabel;
    label->setPixmap(QPixmap::fromImage(content));
    label->setFixedSize(contentSize);
    // The scroll area sits inside a margin: Windows 11 rounds window corners,
    // which would otherwise cut into the bottom rows of the viewport.
    QWidget window;
    window.setWindowFlag(Qt::WindowStaysOnTopHint);
    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(24, 24, 24, 24);
    auto *areaPtr = new QScrollArea;
    QScrollArea &area = *areaPtr;
    area.setWidget(label);
    area.setFrameShape(QFrame::NoFrame);
    area.setFixedSize(contentSize.width() + area.verticalScrollBar()->sizeHint().width(), 420);
    layout->addWidget(areaPtr);
    window.move(QGuiApplication::primaryScreen()->availableGeometry().center() - QPoint(320, 240));
    window.show();
    window.raise();
    window.activateWindow();

    int exitCode = 1;
    QTimer::singleShot(800, &window, [&] {
        // Capture the viewport only (not the scroll bar), as a user would pick it.
        const QRect viewport(area.viewport()->mapToGlobal(QPoint(0, 0)), area.viewport()->size());
        auto *session = new ScrollCaptureSession(viewport, &app);
        QObject::connect(session, &ScrollCaptureSession::finished, &app, [&](const QImage &image) {
            if (!savePath.isEmpty())
                image.save(savePath);
            // The whole content, top to bottom, at device pixels.
            const QImage expected = content.convertToFormat(QImage::Format_RGB32);
            if (!savePath.isEmpty())
                expected.save(savePath + QStringLiteral(".expected.png"));
            const bool sizeOk = image.size() == expected.size();
            int differing = 0;
            QString ranges; // differing rows as "from-to" spans, for diagnosis
            int spanStart = -1;
            if (sizeOk) {
                for (int y = 0; y <= expected.height(); ++y) {
                    const bool differs = y < expected.height()
                                         && std::memcmp(image.constScanLine(y), expected.constScanLine(y),
                                                        size_t(expected.width()) * 4) != 0;
                    differing += differs;
                    if (differs && spanStart < 0)
                        spanStart = y;
                    if (!differs && spanStart >= 0) {
                        ranges += QStringLiteral(" %1-%2").arg(spanStart).arg(y - 1);
                        spanStart = -1;
                    }
                }
            }
            if (sizeOk && differing > 0) {
                int maxDelta = 0; // largest per-channel difference: tint or real content?
                for (int y = 0; y < expected.height(); ++y) {
                    const auto *a = reinterpret_cast<const QRgb *>(image.constScanLine(y));
                    const auto *b = reinterpret_cast<const QRgb *>(expected.constScanLine(y));
                    for (int x = 0; x < expected.width(); ++x)
                        maxDelta = std::max({maxDelta, std::abs(qRed(a[x]) - qRed(b[x])),
                                             std::abs(qGreen(a[x]) - qGreen(b[x])), std::abs(qBlue(a[x]) - qBlue(b[x]))});
                }
                std::printf("largest channel difference: %d\n", maxDelta);
            }
            if (!ranges.isEmpty())
                std::printf("differing rows:%s\n", qPrintable(ranges));
            std::printf("scrolling capture: got %dx%d, expected %dx%d, rows differing: %d -> %s\n", image.width(),
                        image.height(), expected.width(), expected.height(), sizeOk ? differing : -1,
                        sizeOk && differing == 0 ? "PASS" : "FAIL");
            std::fflush(stdout);
            exitCode = sizeOk && differing == 0 ? 0 : 1;
            QApplication::quit();
        });
        session->start();
    });
    QApplication::exec();
    return exitCode;
}
