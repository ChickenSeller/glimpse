#include "ScrollInput.h"

#include <windows.h>

#include <vector>

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

// The integrity level of a process token (its mandatory label's RID), which
// is what Windows compares to let input through (UIPI); -1 if unknown.
long integrityOf(HANDLE token)
{
    DWORD size = 0;
    GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &size);
    if (size == 0)
        return -1;
    std::vector<char> buffer(size);
    auto *label = reinterpret_cast<TOKEN_MANDATORY_LABEL *>(buffer.data());
    if (!GetTokenInformation(token, TokenIntegrityLevel, label, size, &size))
        return -1;
    const DWORD count = *GetSidSubAuthorityCount(label->Label.Sid);
    return long(*GetSidSubAuthority(label->Label.Sid, count - 1));
}

long integrityOfProcess(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return -1;
    long level = -1;
    HANDLE token = nullptr;
    if (OpenProcessToken(process, TOKEN_QUERY, &token)) {
        level = integrityOf(token);
        CloseHandle(token);
    }
    CloseHandle(process);
    return level;
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

bool targetOutOfReach()
{
    POINT point;
    if (!GetCursorPos(&point))
        return false;
    HWND window = WindowFromPoint(point);
    if (!window)
        return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(GetAncestor(window, GA_ROOT), &pid);
    if (pid == 0 || pid == GetCurrentProcessId())
        return false;
    const long own = integrityOfProcess(GetCurrentProcessId());
    // An elevated process does not let a lesser one read its token at all.
    const long target = integrityOfProcess(pid);
    return own >= 0 && own < SECURITY_MANDATORY_HIGH_RID && (target < 0 || target > own);
}

} // namespace ScrollInput
