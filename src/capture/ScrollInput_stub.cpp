#include "ScrollInput.h"

// TODO: X11 via XTest. Wayland does not let clients inject input into others.
namespace ScrollInput {

void setHorizontal(bool)
{
}

void wheel(int)
{
}

bool escapePressed()
{
    return false;
}

void watchUser(bool)
{
}

bool userScrolled()
{
    return false;
}

void userWheel(int)
{
}

void pace(bool)
{
}

int passOnPending(int)
{
    return 0;
}

bool targetOutOfReach()
{
    return false;
}

} // namespace ScrollInput
