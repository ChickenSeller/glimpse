#include "AppIcon.h"

QIcon appIcon()
{
    // Vector artwork, sharp at every size (window, taskbar, tray, About).
    static const QIcon icon(QStringLiteral(":/glimpse.svg"));
    return icon;
}
