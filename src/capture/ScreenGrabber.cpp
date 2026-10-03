#include "ScreenGrabber.h"

#include "QtScreenGrabber.h"

#ifdef GLIMPSE_HAVE_PORTAL
#include "Platform.h"
#include "PortalScreenGrabber.h"
#endif

ScreenGrabber *ScreenGrabber::create(QObject *parent)
{
#ifdef GLIMPSE_HAVE_PORTAL
    if (Platform::displayServer() == Platform::DisplayServer::Wayland)
        return new PortalScreenGrabber(parent);
#endif
    return new QtScreenGrabber(parent);
}
