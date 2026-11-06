#include "ScrollInput.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

namespace {

// Marks the wheel input Glimpse sends, so the hook can tell it from the user's.
constexpr ULONG_PTR kOwnInput = 0x476c6d70; // "Glmp"

std::atomic<bool> g_watching{false};
std::atomic<bool> g_userScrolled{false};
std::atomic<bool> g_pacing{false};
std::atomic<int> g_pending{0}; // wheel units held back while pacing

// A low-level hook sees all mouse input before any window does. Windows
// passes the input on without it when the hook's thread does not answer in
// time, and Glimpse's own thread is busy grabbing and stitching: the hook
// runs on a thread of its own that does nothing else.
LRESULT CALLBACK mouseHook(int code, WPARAM message, LPARAM data)
{
    if (code == HC_ACTION && (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)) {
        const auto *info = reinterpret_cast<const MSLLHOOKSTRUCT *>(data);
        if (info->dwExtraInfo != kOwnInput) {
            if (g_pacing && message == WM_MOUSEWHEEL) {
                g_pending += GET_WHEEL_DELTA_WPARAM(info->mouseData);
                return 1; // held back; passed on by the capture
            }
            if (g_watching)
                g_userScrolled = true;
        }
    }
    return CallNextHookEx(nullptr, code, message, data);
}

std::thread g_hookThread;
std::atomic<DWORD> g_hookThreadId{0};

// The hook (and its thread) is in place while anything needs it. Called from
// Glimpse's thread only.
void updateHook()
{
    const bool needed = g_watching || g_pacing;
    if (needed && !g_hookThread.joinable()) {
        g_hookThreadId = 0;
        g_hookThread = std::thread([] {
            MSG msg;
            PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE); // gives the thread its message queue
            const HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, mouseHook, GetModuleHandleW(nullptr), 0);
            g_hookThreadId = GetCurrentThreadId();
            while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            }
            if (hook)
                UnhookWindowsHookEx(hook);
        });
        while (g_hookThreadId == 0) // until it can be told to quit
            Sleep(1);
    } else if (!needed && g_hookThread.joinable()) {
        PostThreadMessageW(g_hookThreadId, WM_QUIT, 0, 0);
        g_hookThread.join();
    }
}

void sendWheel(int delta, ULONG_PTR extra)
{
    // SendInput goes through the normal input path, so it reaches whatever is
    // under the pointer exactly like a real wheel (posting WM_MOUSEWHEEL does
    // not work for browsers and other apps that read raw input).
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = DWORD(delta);
    input.mi.dwExtraInfo = extra;
    SendInput(1, &input, sizeof(input));
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
    sendWheel(notches * WHEEL_DELTA, kOwnInput);
}

void wheelBy(int delta)
{
    sendWheel(delta, kOwnInput);
}

void userWheel(int notches)
{
    sendWheel(notches * WHEEL_DELTA, 0);
}

void pace(bool on)
{
    g_pacing = on;
    g_pending = 0;
    updateHook();
}

int takePending(int limit)
{
    const int taken = std::clamp(g_pending.load(), -limit, limit);
    g_pending -= taken;
    return taken;
}

bool escapePressed()
{
    // High bit: down now; low bit: pressed since the previous call.
    return GetAsyncKeyState(VK_ESCAPE) & 0x8001;
}

void watchUser(bool on)
{
    g_userScrolled = false;
    g_watching = on;
    updateHook();
}

bool userScrolled()
{
    return g_userScrolled.exchange(false);
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
