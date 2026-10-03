#include "WindowList.h"

// TODO: X11 via _NET_CLIENT_LIST_STACKING. Wayland exposes no other clients'
// windows, so window picking there will stay with the portal's own UI.
std::vector<WindowNode> enumerateWindows()
{
    return {};
}
