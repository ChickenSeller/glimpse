// glimpse-stitch-test: simulates scrolling a tall page through a viewport with
// a sticky header and footer, feeds the frames to ScrollStitcher and checks
// the result is pixel-identical to the page. Exit code 0 on success.

#include "capture/ScrollStitcher.h"

#include <QDir>
#include <QGuiApplication>
#include <QPainter>
#include <QRandomGenerator>
#include <QTransform>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int kWidth = 360;
constexpr int kHeader = 40;
constexpr int kFooter = 28;
constexpr int kContent = 420; // scrolling part of the viewport

// A page with text-like blocks, a long blank band and a few repeated rows,
// the cases that trip naive row matching.
QImage makePage(int height, bool blankBand)
{
    QImage page(kWidth, height, QImage::Format_RGB32);
    page.fill(Qt::white);
    QPainter p(&page);
    QRandomGenerator rng(42);
    for (int y = 0; y < height; y += 18) {
        // Blank band: taller than a scroll step, shorter than the viewport. (A
        // band taller than the viewport cannot be measured by any stitcher: the
        // frames inside it are identical.)
        if (blankBand && y > 1200 && y < 1500)
            continue;
        const int words = 1 + int(rng.bounded(6));
        int x = 8;
        for (int w = 0; w < words && x < kWidth - 20; ++w) {
            const int len = 10 + int(rng.bounded(50));
            p.fillRect(x, y + 4, len, 9, QColor::fromHsv(int(rng.bounded(360)), 120, 90));
            x += len + 6;
        }
    }
    p.fillRect(0, 2400, kWidth, 3, Qt::black); // identical separators
    p.fillRect(0, 2460, kWidth, 3, Qt::black);
    return page;
}

QImage frameAt(const QImage &page, int scroll)
{
    QImage frame(kWidth, kHeader + kContent + kFooter, QImage::Format_RGB32);
    QPainter p(&frame);
    p.fillRect(0, 0, kWidth, kHeader, QColor(30, 60, 140)); // sticky header
    p.fillRect(10, 12, 120, 14, Qt::white);
    p.drawImage(0, kHeader, page, 0, scroll, kWidth, kContent);
    p.fillRect(0, kHeader + kContent, kWidth, kFooter, QColor(220, 220, 220)); // sticky footer
    for (int x = 30; x < 220; x += 38) // its labels
        p.fillRect(x, kHeader + kContent + 9, 24, 10, QColor(60, 60, 60));
    p.fillRect(kWidth - 90, kHeader + kContent + 6, 80, 16, QColor(0, 120, 60));
    return frame;
}

// Negative steps scroll back up (Tracking only); the result covers the page
// down to the furthest point reached.
bool runCase(const char *name, const QList<int> &steps, bool blankBand,
             ScrollStitcher::Scrolling scrolling = ScrollStitcher::Scrolling::Down)
{
    const int pageHeight = 3000;
    const QImage page = makePage(pageHeight, blankBand);
    ScrollStitcher stitcher(scrolling);
    int scroll = 0;
    int furthest = 0;
    stitcher.addFrame(frameAt(page, scroll));
    for (int step : steps) {
        scroll = std::clamp(scroll + step, 0, pageHeight - kContent);
        furthest = std::max(furthest, scroll);
        stitcher.addFrame(frameAt(page, scroll));
    }
    // Reaching the end: further frames are identical.
    const auto end = stitcher.addFrame(frameAt(page, scroll));

    QImage expected(kWidth, kHeader + furthest + kContent + kFooter, QImage::Format_RGB32);
    {
        QPainter p(&expected);
        const QImage first = frameAt(page, 0);
        p.drawImage(0, 0, first, 0, 0, kWidth, kHeader);
        p.drawImage(0, kHeader, page, 0, 0, kWidth, furthest + kContent);
        p.drawImage(0, kHeader + furthest + kContent, first, 0, kHeader + kContent, kWidth, kFooter);
    }
    const QImage actual = stitcher.image();
    const bool ok = actual == expected && end == ScrollStitcher::Result::NoChange;
    std::printf("%-28s %s  (%dx%d, expected %dx%d)\n", name, ok ? "PASS" : "FAIL", actual.width(), actual.height(),
                expected.width(), expected.height());
    return ok;
}

// Page rows of a banner that shows a different slide in every frame.
constexpr int kBannerTop = 500;
constexpr int kBannerHeight = 160;

// A browser-like view: a scroll bar whose thumb moves with every step, a
// navigation bar that turns sticky (covers the top of the view) once scrolled,
// a rotating banner in the left half of the page and a progress bar that
// grows and shrinks in the footer.
QImage browserFrameAt(const QImage &page, int scroll, int pageHeight, int frameIndex)
{
    QImage frame = frameAt(page, scroll);
    QPainter p(&frame);
    const int bannerY = kHeader + kBannerTop - scroll;
    p.setClipRect(0, kHeader, kWidth, kContent);
    p.fillRect(0, bannerY, kWidth / 2, kBannerHeight, QColor::fromHsv((frameIndex * 97) % 360, 200, 220));
    p.fillRect(20, bannerY + 40 + (frameIndex % 3) * 20, 120, 30, Qt::white);
    p.setClipping(false);
    if (scroll > 0)
        p.fillRect(0, kHeader, kWidth, 60, QColor(0, 90, 170)); // sticky nav
    p.fillRect(236, kHeader + kContent + 10, 4 + (frameIndex % 3) * 4, 8, QColor(40, 170, 80)); // progress
    const int track = kContent;
    const int thumb = track * kContent / pageHeight;
    const int thumbTop = (track - thumb) * scroll / (pageHeight - kContent);
    p.fillRect(kWidth - 14, kHeader, 14, track, QColor(240, 240, 240));
    p.fillRect(kWidth - 12, kHeader + thumbTop, 10, thumb, QColor(130, 130, 130));
    return frame;
}

bool runBrowserCase()
{
    const int pageHeight = 3000;
    const QImage page = makePage(pageHeight, false);
    ScrollStitcher stitcher;
    int scroll = 0;
    int frameIndex = 0;
    stitcher.addFrame(browserFrameAt(page, scroll, pageHeight, frameIndex++));
    while (scroll < pageHeight - kContent) {
        scroll = std::min(scroll + 150, pageHeight - kContent);
        stitcher.addFrame(browserFrameAt(page, scroll, pageHeight, frameIndex++));
    }
    // At the end the banner keeps rotating, but it is out of view by then.
    const auto end = stitcher.addFrame(browserFrameAt(page, scroll, pageHeight, frameIndex++));

    // The page content must come out complete; the scroll bar column and the
    // banner are whatever the frames showed, so leave them out.
    const QImage actual = stitcher.image();
    bool ok = end == ScrollStitcher::Result::NoChange && actual.height() == kHeader + pageHeight + kFooter;
    for (int y = 0; ok && y < pageHeight; ++y) {
        const auto *got = reinterpret_cast<const QRgb *>(actual.constScanLine(kHeader + y));
        const auto *want = reinterpret_cast<const QRgb *>(page.constScanLine(y));
        const bool inBanner = y >= kBannerTop && y < kBannerTop + kBannerHeight;
        const int from = inBanner ? kWidth / 2 : 0;
        ok = std::equal(got + from, got + kWidth - 24, want + from);
    }
    std::printf("%-28s %s  (%dx%d, expected height %d)\n", "browser: bar, sticky, banner", ok ? "PASS" : "FAIL",
                actual.width(), actual.height(), kHeader + pageHeight + kFooter);
    return ok;
}

// The horizontal shift from a to b (positive: b shows the view scrolled
// right), by the least mean difference of a sample of pixels: slow, but
// tolerant of anything that is not exactly equal.
int bruteShiftX(const QImage &a, const QImage &b, int range)
{
    int best = 0;
    double bestDiff = 1e30;
    for (int dx = -range; dx <= range; ++dx) {
        double diff = 0;
        long n = 0;
        for (int y = 0; y < a.height(); y += 6) {
            const auto *ra = reinterpret_cast<const QRgb *>(a.constScanLine(y));
            const auto *rb = reinterpret_cast<const QRgb *>(b.constScanLine(y));
            for (int x = std::max(0, -dx); x < std::min(a.width(), a.width() - dx); x += 3) {
                diff += std::abs(qGray(ra[x + dx]) - qGray(rb[x]));
                ++n;
            }
        }
        if (n > 0 && diff / n < bestDiff) {
            bestDiff = diff / n;
            best = dx;
        }
    }
    return best;
}

// --replay <dir> [--vertical]: feeds the frames a capture saved with
// QT_LOGGING_RULES="glimpse.scroll.debug=true" (glimpse-manual-*.png) to a
// Tracking stitcher, printing what it made of each, and saves the result.
int replay(const QString &dir, bool horizontal)
{
    const QDir frames(dir);
    const QStringList files = frames.entryList({QStringLiteral("glimpse-manual-*.png")}, QDir::Files, QDir::Name);
    ScrollStitcher stitcher(ScrollStitcher::Scrolling::Tracking,
                            horizontal ? ScrollStitcher::Direction::Horizontal : ScrollStitcher::Direction::Vertical);
    QImage previous;
    for (const QString &file : files) {
        const QImage frame = QImage(frames.filePath(file)).convertToFormat(QImage::Format_RGB32);
        const auto result = stitcher.addFrame(frame);
        const int truth = previous.isNull() || !horizontal ? 0 : bruteShiftX(previous, frame, frame.width() / 3);
        // How much of the overlap is exactly the same at the true shift.
        long same = 0, all = 0;
        if (truth != 0) {
            for (int y = 0; y < frame.height(); ++y) {
                const auto *ra = reinterpret_cast<const QRgb *>(previous.constScanLine(y));
                const auto *rb = reinterpret_cast<const QRgb *>(frame.constScanLine(y));
                for (int x = std::max(0, -truth); x < std::min(frame.width(), frame.width() - truth); ++x) {
                    ++all;
                    same += ra[x + truth] == rb[x];
                }
            }
        }
        std::printf("%s  result %d  shift %5d  true %5d  exact %5.1f%%  length %d\n", qPrintable(file), int(result),
                    stitcher.lastShift(), truth, all ? 100.0 * same / all : 0.0,
                    horizontal ? stitcher.image().width() : stitcher.height());
        if (result == ScrollStitcher::Result::Added || result == ScrollStitcher::Result::Moved)
            previous = frame;
        else if (previous.isNull())
            previous = frame;
    }
    stitcher.image().save(frames.filePath(QStringLiteral("replay-result.png")));
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    const QStringList args = app.arguments();
    // --diff a.png b.png <out.png>: b lined up on a by the true horizontal
    // shift, pixels that differ in red over a dimmed b.
    if (args.size() >= 5 && args[1] == QLatin1String("--diff")) {
        const QImage a = QImage(args[2]).convertToFormat(QImage::Format_RGB32);
        const QImage b = QImage(args[3]).convertToFormat(QImage::Format_RGB32);
        const int dx = bruteShiftX(a, b, a.width() / 3);
        QImage out(b.size(), QImage::Format_RGB32);
        for (int y = 0; y < b.height(); ++y) {
            const auto *ra = reinterpret_cast<const QRgb *>(a.constScanLine(y));
            const auto *rb = reinterpret_cast<const QRgb *>(b.constScanLine(y));
            auto *ro = reinterpret_cast<QRgb *>(out.scanLine(y));
            for (int x = 0; x < b.width(); ++x) {
                const int g = qGray(rb[x]) / 3;
                const bool inside = x + dx >= 0 && x + dx < a.width();
                ro[x] = !inside ? qRgb(0, 0, 80) : ra[x + dx] == rb[x] ? qRgb(g, g, g) : qRgb(255, 0, 0);
            }
        }
        out.save(args[4]);
        std::printf("shift %d\n", dx);
        return 0;
    }
    if (args.size() >= 3 && args[1] == QLatin1String("--replay"))
        return replay(args[2], !args.contains(QLatin1String("--vertical")));
    bool ok = true;
    // Real scrolling moves a fixed number of wheel notches per step.
    ok &= runCase("fixed steps, blank band", QList<int>(20, 150), true);
    ok &= runCase("fixed steps, no blank band", QList<int>(20, 150), false);
    // Varying steps can only be measured from content, so no blank band here.
    ok &= runCase("uneven steps", {120, 77, 201, 33, 180, 160, 199, 64, 150, 211, 90, 170, 188, 140, 205, 120,
                                   175, 166, 200, 210, 190}, false);
    ok &= runCase("small steps, blank band", QList<int>(60, 45), true);
    ok &= runBrowserCase();
    // The user scrolling: uneven steps, back up and down again; -400 leaves
    // too little overlap and is lost, until the view is back within reach.
    ok &= runCase("tracking, up and down", {120, 77, -150, 201, 33, -300, 180, 260, 160, -60, 199, 64, 150, -400, 211,
                                            90, 360, 170, 188, 140, 205, 120, 175, 166, 200, 210, 190},
                  false, ScrollStitcher::Scrolling::Tracking);
    // Undo while the user scrolls: the rows come back once the view moves on,
    // and after undoing past the view, only once it is back within reach.
    {
        const QImage page = makePage(3000, false);
        ScrollStitcher stitcher(ScrollStitcher::Scrolling::Tracking);
        using R = ScrollStitcher::Result;
        bool ok2 = true;
        for (int scroll : {0, 120, 240})
            stitcher.addFrame(frameAt(page, scroll));
        const int before = stitcher.height();
        ok2 &= stitcher.undoLast() && stitcher.height() == before - 120;
        ok2 &= stitcher.addFrame(frameAt(page, 260)) == R::Added; // 120 back, 20 new
        ok2 &= stitcher.undoLast() && stitcher.undoLast() && stitcher.canUndo() == false;
        ok2 &= !stitcher.undoLast(); // the first frame stays
        ok2 &= stitcher.addFrame(frameAt(page, 560)) == R::Gap; // rows 420..560 never seen together
        ok2 &= stitcher.addFrame(frameAt(page, 380)) == R::Added;
        ok2 &= stitcher.addFrame(frameAt(page, 380)) == R::NoChange;
        // As if scrolled straight to 380.
        QImage expected(kWidth, kHeader + 380 + kContent + kFooter, QImage::Format_RGB32);
        {
            QPainter p(&expected);
            const QImage first = frameAt(page, 0);
            p.drawImage(0, 0, first, 0, 0, kWidth, kHeader);
            p.drawImage(0, kHeader, page, 0, 0, kWidth, 380 + kContent);
            p.drawImage(0, kHeader + 380 + kContent, first, 0, kHeader + kContent, kWidth, kFooter);
        }
        ok2 &= stitcher.image() == expected;
        std::printf("%-28s %s\n", "undo, then scroll on", ok2 ? "PASS" : "FAIL");
        ok &= ok2;

        // The preview fits the size asked for and keeps the proportions.
        const QImage preview = stitcher.preview(QSize(200, 300));
        const QImage full = stitcher.image();
        const bool fits = preview.width() <= 200 && preview.height() == 300
                          && std::abs(preview.width() - full.width() * 300 / full.height()) <= 1;
        std::printf("%-28s %s  (%dx%d)\n", "preview fits", fits ? "PASS" : "FAIL", preview.width(), preview.height());
        ok &= fits;
    }
    // Scrolling to the right: the same page turned on its side (header at the
    // left, footer at the right), scrolled back and forth by the user.
    {
        const auto turned = [](const QImage &image) {
            return image.transformed(QTransform(0, 1, 1, 0, 0, 0)).convertToFormat(QImage::Format_RGB32);
        };
        const QImage page = makePage(3000, false);
        ScrollStitcher stitcher(ScrollStitcher::Scrolling::Tracking, ScrollStitcher::Direction::Horizontal);
        int scroll = 0, furthest = 0;
        for (int step : {0, 120, 77, -150, 201, 33, 180, 260, 160, -60, 199, 64, 150}) {
            scroll = std::max(0, scroll + step);
            furthest = std::max(furthest, scroll);
            stitcher.addFrame(turned(frameAt(page, scroll)));
        }
        const auto end = stitcher.addFrame(turned(frameAt(page, scroll)));
        QImage expected(kWidth, kHeader + furthest + kContent + kFooter, QImage::Format_RGB32);
        {
            QPainter p(&expected);
            const QImage first = frameAt(page, 0);
            p.drawImage(0, 0, first, 0, 0, kWidth, kHeader);
            p.drawImage(0, kHeader, page, 0, 0, kWidth, furthest + kContent);
            p.drawImage(0, kHeader + furthest + kContent, first, 0, kHeader + kContent, kWidth, kFooter);
        }
        const QImage actual = stitcher.image();
        const bool sideways = end == ScrollStitcher::Result::NoChange && actual == turned(expected);
        std::printf("%-28s %s  (%dx%d)\n", "horizontal, tracking", sideways ? "PASS" : "FAIL", actual.width(),
                    actual.height());
        ok &= sideways;

        const QImage preview = stitcher.preview(QSize(300, 200));
        const bool fits = preview.height() <= 200 && preview.width() == 300
                          && std::abs(preview.height() - actual.height() * 300 / actual.width()) <= 1;
        std::printf("%-28s %s  (%dx%d)\n", "horizontal preview fits", fits ? "PASS" : "FAIL", preview.width(),
                    preview.height());
        ok &= fits;
    }
    // A dithered background, fixed to the screen while the page scrolls over
    // it (browsers dither gradients): white a step darker in a pattern.
    {
        const auto dithered = [](QImage frame) {
            for (int y = 0; y < frame.height(); ++y) {
                auto *row = reinterpret_cast<QRgb *>(frame.scanLine(y));
                for (int x = 0; x < frame.width(); ++x) {
                    if (row[x] == qRgb(255, 255, 255) && (x * 7 + y * 3) % 5 < 2)
                        row[x] = qRgb(255, 255, 254);
                }
            }
            return frame;
        };
        const QImage page = makePage(3000, false);
        ScrollStitcher stitcher(ScrollStitcher::Scrolling::Tracking);
        int scroll = 0, furthest = 0;
        for (int step : {0, 120, 77, -150, 201, 33, 180, 260, 160, -60, 199, 64, 150}) {
            scroll = std::max(0, scroll + step);
            furthest = std::max(furthest, scroll);
            stitcher.addFrame(dithered(frameAt(page, scroll)));
        }
        const auto end = stitcher.addFrame(dithered(frameAt(page, scroll)));
        const QImage actual = stitcher.image();
        bool same = end == ScrollStitcher::Result::NoChange && actual.height() == kHeader + furthest + kContent + kFooter;
        for (int y = 0; same && y < furthest + kContent; ++y) {
            const auto *got = reinterpret_cast<const QRgb *>(actual.constScanLine(kHeader + y));
            const auto *want = reinterpret_cast<const QRgb *>(page.constScanLine(y));
            for (int x = 0; same && x < kWidth; ++x)
                same = std::abs(qBlue(got[x]) - qBlue(want[x])) <= 1 && qRed(got[x]) == qRed(want[x]);
        }
        std::printf("%-28s %s  (%dx%d)\n", "dithered background", same ? "PASS" : "FAIL", actual.width(),
                    actual.height());
        ok &= same;
    }
    // Overshooting step: the stitcher must refuse it rather than stitch garbage.
    {
        const QImage page = makePage(3000, false);
        ScrollStitcher stitcher;
        stitcher.addFrame(frameAt(page, 0));
        const auto result = stitcher.addFrame(frameAt(page, kContent + 50));
        const bool refused = result == ScrollStitcher::Result::NoOverlap;
        std::printf("%-28s %s\n", "overshoot is refused", refused ? "PASS" : "FAIL");
        ok &= refused;
    }
    return ok ? 0 : 1;
}
