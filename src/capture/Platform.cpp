#include "Platform.h"

#include <QtGlobal>

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

} // namespace Platform
