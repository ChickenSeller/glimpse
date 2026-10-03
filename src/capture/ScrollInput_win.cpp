#include "ScrollInput.h"

#include <windows.h>

namespace ScrollInput {

void wheel(int notches)
{
    // SendInput goes through the normal input path, so it reaches whatever is
    // under the pointer exactly like a real wheel (posting WM_MOUSEWHEEL does
    // not work for browsers and other apps that read raw input).
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = DWORD(notches * WHEEL_DELTA);
    SendInput(1, &input, sizeof(input));
}

bool escapePressed()
{
    // High bit: down now; low bit: pressed since the previous call.
    return GetAsyncKeyState(VK_ESCAPE) & 0x8001;
}

} // namespace ScrollInput
