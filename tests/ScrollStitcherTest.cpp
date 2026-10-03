// glimpse-stitch-test: simulates scrolling a tall page through a viewport with
// a sticky header and footer, feeds the frames to ScrollStitcher and checks
// the result is pixel-identical to the page. Exit code 0 on success.

#include "capture/ScrollStitcher.h"

#include <QGuiApplication>
#include <QPainter>
#include <QRandomGenerator>

#include <cstdio>

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
    p.fillRect(kWidth - 90, kHeader + kContent + 6, 80, 16, QColor(0, 120, 60));
    return frame;
}

bool runCase(const char *name, const QList<int> &steps, bool blankBand)
{
    const int pageHeight = 3000;
    const QImage page = makePage(pageHeight, blankBand);
    ScrollStitcher stitcher;
    int scroll = 0;
    stitcher.addFrame(frameAt(page, scroll));
    for (int step : steps) {
        scroll = std::min(scroll + step, pageHeight - kContent);
        stitcher.addFrame(frameAt(page, scroll));
    }
    // Reaching the end: further frames are identical.
    const auto end = stitcher.addFrame(frameAt(page, scroll));

    QImage expected(kWidth, kHeader + scroll + kContent + kFooter, QImage::Format_RGB32);
    {
        QPainter p(&expected);
        const QImage first = frameAt(page, 0);
        p.drawImage(0, 0, first, 0, 0, kWidth, kHeader);
        p.drawImage(0, kHeader, page, 0, 0, kWidth, scroll + kContent);
        p.drawImage(0, kHeader + scroll + kContent, first, 0, kHeader + kContent, kWidth, kFooter);
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
// navigation bar that turns sticky (covers the top of the view) once scrolled
// and a rotating banner in the left half of the page.
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

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    bool ok = true;
    // Real scrolling moves a fixed number of wheel notches per step.
    ok &= runCase("fixed steps, blank band", QList<int>(20, 150), true);
    ok &= runCase("fixed steps, no blank band", QList<int>(20, 150), false);
    // Varying steps can only be measured from content, so no blank band here.
    ok &= runCase("uneven steps", {120, 77, 201, 33, 180, 160, 199, 64, 150, 211, 90, 170, 188, 140, 205, 120,
                                   175, 166, 200, 210, 190}, false);
    ok &= runCase("small steps, blank band", QList<int>(60, 45), true);
    ok &= runBrowserCase();
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
