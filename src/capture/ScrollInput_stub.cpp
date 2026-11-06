#include "ScrollInput.h"

// TODO: X11 via XTest. Wayland does not let clients inject input into others.
namespace ScrollInput {

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

void wheelBy(int)
{
}

void userWheel(int)
{
}

void pace(bool)
{
}

int takePending(int)
{
    return 0;
}

bool targetOutOfReach()
{
    return false;
}

} // namespace ScrollInput
