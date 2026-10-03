#include "Platform.h"

#include <QCoreApplication>
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
    return displayServer() == DisplayServer::Windows;
}

bool supportsScrollingCapture()
{
    return displayServer() == DisplayServer::Windows;
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

std::optional<QPoint> nativeCursorPos()
{
#ifdef Q_OS_WIN
    POINT point;
    if (GetCursorPos(&point)) // physical: Qt makes the process per-monitor DPI aware
        return QPoint(point.x, point.y);
#endif
    return std::nullopt;
}

QString unsupportedHint()
{
    return QCoreApplication::translate("Platform", "Not available on %1 yet.").arg(displayServerName());
}

} // namespace Platform
