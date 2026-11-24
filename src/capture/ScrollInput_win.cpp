#include "ScrollInput.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

namespace {

// Marks the wheel input Glimpse sends, so the hook can tell it from the user's.
constexpr ULONG_PTR kOwnInput = 0x476c6d70; // "Glmp"

std::atomic<bool> g_horizontal{false};
std::atomic<bool> g_watching{false};
std::atomic<bool> g_userScrolled{false};
std::atomic<bool> g_pacing{false};
// Wheel units held back while pacing, as they came: of the vertical wheel
// (horizontally, those turned with Shift held), and of the horizontal one.
std::atomic<int> g_pending{0};
std::atomic<int> g_pendingH{0};

// A low-level hook sees all mouse input before any window does. Windows
// passes the input on without it when the hook's thread does not answer in
// time, and Glimpse's own thread is busy grabbing and stitching: the hook
// runs on a thread of its own that does nothing else.
LRESULT CALLBACK mouseHook(int code, WPARAM message, LPARAM data)
{
    if (code == HC_ACTION && (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)) {
        const auto *info = reinterpret_cast<const MSLLHOOKSTRUCT *>(data);
        if (info->dwExtraInfo != kOwnInput) {
            if (g_pacing) {
                const int delta = GET_WHEEL_DELTA_WPARAM(info->mouseData);
                if (!g_horizontal) {
                    if (message == WM_MOUSEWHEEL) {
                        g_pending += delta;
                        return 1; // held back; passed on by the capture
                    }
                } else if (message == WM_MOUSEHWHEEL) {
                    g_pendingH += delta;
                    return 1;
                } else if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                    g_pending += delta; // Shift+wheel scrolls sideways
                    return 1;
                }
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

// The horizontal wheel's units are positive to the right, unlike the vertical
// one's, which are positive up: `delta` is in the vertical wheel's sense.
void sendWheel(int delta, ULONG_PTR extra, bool horizontal = g_horizontal)
{
    // SendInput goes through the normal input path, so it reaches whatever is
    // under the pointer exactly like a real wheel (posting WM_MOUSEWHEEL does
    // not work for browsers and other apps that read raw input).
    if (horizontal)
        delta = -delta;
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = horizontal ? MOUSEEVENTF_HWHEEL : MOUSEEVENTF_WHEEL;
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

void setHorizontal(bool horizontal)
{
    g_horizontal = horizontal;
    g_pending = 0;
    g_pendingH = 0;
}

void wheel(int notches)
{
    sendWheel(notches * WHEEL_DELTA, kOwnInput);
}

void userWheel(int notches)
{
    sendWheel(notches * WHEEL_DELTA, 0);
}

void pace(bool on)
{
    g_pacing = on;
    g_pending = 0;
    g_pendingH = 0;
    updateHook();
}

int passOnPending(int limit)
{
    // The horizontal wheel's own units first, sent back as they came; then the
    // vertical wheel's, also as they came (with Shift still held, they scroll
    // sideways again).
    if (const int h = std::clamp(g_pendingH.load(), -limit, limit)) {
        g_pendingH -= h;
        sendWheel(-h, kOwnInput, true);
        return -h;
    }
    const int v = std::clamp(g_pending.load(), -limit, limit);
    if (v != 0) {
        g_pending -= v;
        sendWheel(v, kOwnInput, false);
    }
    return v;
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
