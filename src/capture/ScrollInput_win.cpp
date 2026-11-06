#include "ScrollInput.h"

#include <windows.h>

namespace {

// Marks the wheel input Glimpse sends, so the hook can tell it from the user's.
constexpr ULONG_PTR kOwnInput = 0x476c6d70; // "Glmp"

HHOOK g_hook = nullptr;
bool g_userScrolled = false;

// A low-level hook sees all mouse input before any window does; it runs on
// this (the GUI) thread, between its messages.
LRESULT CALLBACK mouseHook(int code, WPARAM message, LPARAM data)
{
    if (code == HC_ACTION && (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)) {
        const auto *info = reinterpret_cast<const MSLLHOOKSTRUCT *>(data);
        if (info->dwExtraInfo != kOwnInput)
            g_userScrolled = true;
    }
    return CallNextHookEx(nullptr, code, message, data);
}

} // namespace

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
    input.mi.dwExtraInfo = kOwnInput;
    SendInput(1, &input, sizeof(input));
}

bool escapePressed()
{
    // High bit: down now; low bit: pressed since the previous call.
    return GetAsyncKeyState(VK_ESCAPE) & 0x8001;
}

void watchUser(bool on)
{
    g_userScrolled = false;
    if (on && !g_hook) {
        g_hook = SetWindowsHookExW(WH_MOUSE_LL, mouseHook, GetModuleHandleW(nullptr), 0);
    } else if (!on && g_hook) {
        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }
}

bool userScrolled()
{
    const bool scrolled = g_userScrolled;
    g_userScrolled = false;
    return scrolled;
}

} // namespace ScrollInput
