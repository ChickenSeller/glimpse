#include "CursorCapture.h"

#include <QGuiApplication>
#include <QScreen>
#include <QtGui/qscreen_platform.h>

#include <windows.h>

#include <algorithm>
#include <cstring>

namespace {

QSize cursorSize(HCURSOR cursor, QPoint *hotspot)
{
    ICONINFO info{};
    if (!GetIconInfo(cursor, &info))
        return {};
    BITMAP bitmap{};
    QSize size;
    if (GetObject(info.hbmColor ? info.hbmColor : info.hbmMask, sizeof(bitmap), &bitmap)) {
        // A monochrome cursor stacks its AND and XOR masks in one bitmap.
        size = QSize(bitmap.bmWidth, info.hbmColor ? bitmap.bmHeight : bitmap.bmHeight / 2);
    }
    *hotspot = QPoint(int(info.xHotspot), int(info.yHotspot));
    if (info.hbmColor)
        DeleteObject(info.hbmColor);
    if (info.hbmMask)
        DeleteObject(info.hbmMask);
    return size;
}

} // namespace

void drawCursor(DesktopSnapshot &snapshot)
{
    CURSORINFO cursor{};
    cursor.cbSize = sizeof(cursor);
    if (!GetCursorInfo(&cursor) || !(cursor.flags & CURSOR_SHOWING) || !cursor.hCursor)
        return;

    // The screen image under the pointer and the pointer's place in it, in
    // physical pixels relative to that monitor.
    const HMONITOR monitor = MonitorFromPoint(cursor.ptScreenPos, MONITOR_DEFAULTTONULL);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo))
        return;
    ScreenImage *target = nullptr;
    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        auto *native = screen->nativeInterface<QNativeInterface::QWindowsScreen>();
        if (!native || native->handle() != monitor)
            continue;
        for (ScreenImage &shot : snapshot.screens) {
            if (shot.name == screen->name())
                target = &shot;
        }
    }
    if (!target)
        return;

    QPoint hotspot;
    const QSize size = cursorSize(cursor.hCursor, &hotspot);
    if (size.isEmpty())
        return;
    const QPoint topLeft(cursor.ptScreenPos.x - monitorInfo.rcMonitor.left - hotspot.x(),
                         cursor.ptScreenPos.y - monitorInfo.rcMonitor.top - hotspot.y());
    QImage &image = target->image;
    if (image.format() != QImage::Format_RGB32 && image.format() != QImage::Format_ARGB32
        && image.format() != QImage::Format_ARGB32_Premultiplied) {
        const qreal dpr = image.devicePixelRatio();
        image = image.convertToFormat(QImage::Format_RGB32);
        image.setDevicePixelRatio(dpr);
    }
    const QRect area = QRect(topLeft, size).intersected(image.rect());
    if (area.isEmpty())
        return;

    // Let GDI draw the cursor onto a copy of the pixels beneath it: DrawIconEx
    // handles every cursor kind, including inverting monochrome ones (I-beam).
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = size.width();
    bmi.bmiHeader.biHeight = -size.height(); // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP dib = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dc || !dib || !bits) {
        if (dib)
            DeleteObject(dib);
        if (dc)
            DeleteDC(dc);
        return;
    }
    HGDIOBJ previous = SelectObject(dc, dib);

    auto *pixels = static_cast<uchar *>(bits);
    const int stride = size.width() * 4;
    const QPoint offset = area.topLeft() - topLeft;
    for (int y = 0; y < area.height(); ++y)
        std::memcpy(pixels + (offset.y() + y) * stride + offset.x() * 4,
                    image.constScanLine(area.top() + y) + area.left() * 4, size_t(area.width()) * 4);

    DrawIconEx(dc, 0, 0, cursor.hCursor, size.width(), size.height(), 0, nullptr, DI_NORMAL);
    GdiFlush();

    for (int y = 0; y < area.height(); ++y) {
        auto *row = reinterpret_cast<quint32 *>(image.scanLine(area.top() + y)) + area.left();
        std::memcpy(row, pixels + (offset.y() + y) * stride + offset.x() * 4, size_t(area.width()) * 4);
        // GDI leaves the alpha byte undefined; the screen image is opaque.
        for (int x = 0; x < area.width(); ++x)
            row[x] |= 0xff000000u;
    }

    SelectObject(dc, previous);
    DeleteObject(dib);
    DeleteDC(dc);
}
