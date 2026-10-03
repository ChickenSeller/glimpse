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

} // namespace ScrollInput
