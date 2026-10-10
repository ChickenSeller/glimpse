#include "Platform.h"

#include <QCoreApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QWidget>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace Platform {

DisplayServer displayServer()
{
#if defined(Q_OS_WIN)
    return DisplayServer::Windows;
#elif defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    static const DisplayServer server = [] {
        const QString session = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();
        if (session == QLatin1String("wayland") || qEnvironmentVariableIsSet("WAYLAND_DISPLAY"))
            return DisplayServer::Wayland;
        if (session == QLatin1String("x11") || qEnvironmentVariableIsSet("DISPLAY"))
            return DisplayServer::X11;
        return DisplayServer::Unknown;
    }();
    return server;
#else
    return DisplayServer::Unknown;
#endif
}

QString displayServerName()
{
    switch (displayServer()) {
    case DisplayServer::Windows: return QStringLiteral("Windows");
    case DisplayServer::X11: return QStringLiteral("X11");
    case DisplayServer::Wayland: return QStringLiteral("Wayland");
    case DisplayServer::Unknown: break;
    }
    return QStringLiteral("Unknown");
}

bool windowsCanPlaceThemselves()
{
    return QGuiApplication::platformName() != QLatin1String("wayland");
}

// TODO: X11 (window tree, XFixes cursor, XGrabKey) and Wayland (GlobalShortcuts
// portal). Wayland cannot offer window picking or the pointer at all.
bool supportsWindowPicking()
{
    return displayServer() == DisplayServer::Windows;
}

bool supportsCursorCapture()
{
    return displayServer() == DisplayServer::Windows;
}

bool supportsGlobalHotkeys()
{
#ifdef Q_OS_LINUX
    // As custom keyboard shortcuts in GNOME's settings (GlobalHotkeys_gnome.cpp).
    static const bool gnome = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':')).contains(QLatin1String("GNOME"))
                              && !QStandardPaths::findExecutable(QStringLiteral("gsettings")).isEmpty()
                              && !QStandardPaths::findExecutable(QStringLiteral("dconf")).isEmpty();
    return gnome;
#else
    return displayServer() == DisplayServer::Windows;
#endif
}

bool supportsScrollingCapture()
{
    return displayServer() == DisplayServer::Windows;
}

bool hidesWindowsInstantly()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

void flushCompositor()
{
#ifdef Q_OS_WIN
    // Each call waits for the next frame DWM presents; the second makes sure
    // the frame was composed after the change, not already on its way.
    DwmFlush();
    DwmFlush();
#endif
}

void disableWindowAnimations(QWidget *window)
{
#ifdef Q_OS_WIN
    const BOOL disable = TRUE;
    DwmSetWindowAttribute(reinterpret_cast<HWND>(window->winId()), DWMWA_TRANSITIONS_FORCEDISABLED, &disable,
                          sizeof(disable));
#else
    Q_UNUSED(window)
#endif
}

bool supportsInputOverlay()
{
    return displayServer() == DisplayServer::Windows;
}

bool supportsPointerHighlight()
{
    return displayServer() == DisplayServer::Windows || displayServer() == DisplayServer::X11;
}

void excludeFromCapture(QWidget *window)
{
#ifdef Q_OS_WIN
    // WDA_EXCLUDEFROMCAPTURE (Windows 10 2004+); older systems ignore it.
    constexpr DWORD kExcludeFromCapture = 0x11;
    SetWindowDisplayAffinity(reinterpret_cast<HWND>(window->winId()), kExcludeFromCapture);
#else
    Q_UNUSED(window);
#endif
}

std::optional<QPoint> nativeCursorPos()
{
#ifdef Q_OS_WIN
    POINT point;
    if (GetCursorPos(&point)) // physical: Qt makes the process per-monitor DPI aware
        return QPoint(point.x, point.y);
#endif
    return std::nullopt;
}

bool moveCursorBy(int dx, int dy)
{
#ifdef Q_OS_WIN
    // Physical pixels: QCursor works in logical ones, rounded at 150% and the like.
    POINT point;
    return GetCursorPos(&point) && SetCursorPos(point.x + dx, point.y + dy);
#else
    if (displayServer() != DisplayServer::X11)
        return false;
    QCursor::setPos(QCursor::pos() + QPoint(dx, dy));
    return true;
#endif
}

QString unsupportedHint()
{
    return QCoreApplication::translate("Platform", "Not available on %1 yet.").arg(displayServerName());
}

} // namespace Platform
