#include "InputMonitor.h"

#include <QHash>
#include <QMetaObject>
#include <QStringList>

#include <windows.h>

namespace {

class WinInputMonitor;
// Low-level hook procedures get no context pointer.
WinInputMonitor *g_monitor = nullptr;

bool isModifier(DWORD vk)
{
    switch (vk) {
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
    case VK_MENU: case VK_LMENU: case VK_RMENU:
    case VK_LWIN: case VK_RWIN:
        return true;
    default:
        return false;
    }
}

QString keyName(const KBDLLHOOKSTRUCT &key)
{
    const DWORD vk = key.vkCode;
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z'))
        return QString(QChar(char(vk)));
    switch (vk) {
    case VK_SPACE: return QStringLiteral("Space");
    case VK_RETURN: return QStringLiteral("Enter");
    case VK_ESCAPE: return QStringLiteral("Esc");
    case VK_TAB: return QStringLiteral("Tab");
    case VK_BACK: return QStringLiteral("Backspace");
    case VK_DELETE: return QStringLiteral("Delete");
    case VK_LEFT: return QStringLiteral("←");
    case VK_UP: return QStringLiteral("↑");
    case VK_RIGHT: return QStringLiteral("→");
    case VK_DOWN: return QStringLiteral("↓");
    default: break;
    }
    if (vk >= VK_F1 && vk <= VK_F24)
        return QStringLiteral("F%1").arg(vk - VK_F1 + 1);
    // Everything else by the keyboard layout's own name for it.
    LONG lParam = LONG(key.scanCode) << 16;
    if (key.flags & LLKHF_EXTENDED)
        lParam |= 1 << 24;
    wchar_t name[64] = {};
    if (GetKeyNameTextW(lParam, name, 64) > 0)
        return QString::fromWCharArray(name);
    return QStringLiteral("0x%1").arg(vk, 2, 16, QLatin1Char('0'));
}

// Where the key is: its scan code, 0xE000 added for extended keys.
int keyId(const KBDLLHOOKSTRUCT &key)
{
    if (key.vkCode == VK_PAUSE)
        return 0xE11D;
    if (key.scanCode == 0) // made up by software: where it would be
        return int(MapVirtualKeyW(key.vkCode, MAPVK_VK_TO_VSC_EX));
    return int(key.scanCode) | ((key.flags & LLKHF_EXTENDED) ? 0xE000 : 0);
}

bool held(int vk)
{
    return GetAsyncKeyState(vk) & 0x8000;
}

class WinInputMonitor : public InputMonitor
{
public:
    using InputMonitor::InputMonitor;
    ~WinInputMonitor() override { stop(); }

    bool isSupported() const override { return true; }

    void start() override
    {
        if (m_mouseHook)
            return;
        g_monitor = this;
        // Called on this (the GUI) thread's message loop; they must return quickly.
        m_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, &WinInputMonitor::mouseProc, GetModuleHandleW(nullptr), 0);
        m_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, &WinInputMonitor::keyboardProc, GetModuleHandleW(nullptr), 0);
    }

    void stop() override
    {
        if (m_mouseHook)
            UnhookWindowsHookEx(m_mouseHook);
        if (m_keyboardHook)
            UnhookWindowsHookEx(m_keyboardHook);
        m_mouseHook = m_keyboardHook = nullptr;
        if (g_monitor == this)
            g_monitor = nullptr;
    }

private:
    static LRESULT CALLBACK mouseProc(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code == HC_ACTION && g_monitor) {
            Qt::MouseButton button = Qt::NoButton;
            bool pressed = false;
            switch (wParam) {
            case WM_LBUTTONDOWN: button = Qt::LeftButton; pressed = true; break;
            case WM_LBUTTONUP: button = Qt::LeftButton; break;
            case WM_RBUTTONDOWN: button = Qt::RightButton; pressed = true; break;
            case WM_RBUTTONUP: button = Qt::RightButton; break;
            case WM_MBUTTONDOWN: button = Qt::MiddleButton; pressed = true; break;
            case WM_MBUTTONUP: button = Qt::MiddleButton; break;
            default: break;
            }
            if (button != Qt::NoButton) {
                QMetaObject::invokeMethod(g_monitor, [button, pressed] {
                    if (g_monitor)
                        emit g_monitor->mouseButton(button, pressed);
                }, Qt::QueuedConnection);
            }
        }
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    static LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code == HC_ACTION && g_monitor) {
            const auto *key = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);
            const bool down = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
            const int id = keyId(*key);
            QMetaObject::invokeMethod(g_monitor, [id, down] {
                if (g_monitor)
                    emit g_monitor->keyState(id, down);
            }, Qt::QueuedConnection);
        }
        if (code == HC_ACTION && g_monitor && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
            const auto *key = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);
            if (!isModifier(key->vkCode)) {
                QStringList parts;
                if (held(VK_CONTROL))
                    parts << QStringLiteral("Ctrl");
                if (held(VK_MENU))
                    parts << QStringLiteral("Alt");
                if (held(VK_SHIFT))
                    parts << QStringLiteral("Shift");
                if (held(VK_LWIN) || held(VK_RWIN))
                    parts << QStringLiteral("Win");
                parts << keyName(*key);
                const QString text = parts.join(QStringLiteral(" + "));
                QMetaObject::invokeMethod(g_monitor, [text] {
                    if (g_monitor)
                        emit g_monitor->keyPressed(text);
                }, Qt::QueuedConnection);
            }
        }
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }

    HHOOK m_mouseHook = nullptr;
    HHOOK m_keyboardHook = nullptr;
};

} // namespace

namespace {

// What a key types, on its own or with Shift.
QString typedBy(UINT vk, int scan, HKL layout, bool shift)
{
    BYTE state[256] = {};
    if (shift)
        state[VK_SHIFT] = 0x80;
    wchar_t text[8] = {};
    // 0x4: leave the keyboard state (dead keys) as it is (Windows 10 1607+).
    int n = ToUnicodeEx(vk, UINT(scan), state, text, 8, 0x4, layout);
    if (n < 0)
        n = 1; // a dead key: its accent
    const QString typed = QString::fromWCharArray(text, std::max(0, n)).trimmed();
    if (typed.isEmpty() || !typed.at(0).isPrint())
        return {};
    return typed;
}

// A key's label: what it types on its own; letters, as on key caps, in
// upper case, which is what they type with Shift (é on a French number key
// types 2 with Shift: it stays é).
QString labelOf(int scan, HKL layout)
{
    const UINT vk = MapVirtualKeyExW(UINT(scan), MAPVK_VSC_TO_VK_EX, layout);
    if (vk == 0)
        return {};
    const QString plain = typedBy(vk, scan, layout, false);
    const QString upper = plain.toUpper();
    return upper != plain && typedBy(vk, scan, layout, true) == upper ? upper : plain;
}

KeyboardLayout layoutOf(HKL hkl)
{
    static QHash<quintptr, KeyboardLayout> known;
    const auto id = quintptr(hkl);
    if (const auto it = known.constFind(id); it != known.constEnd())
        return *it;

    KeyboardLayout layout;
    layout.id = id;
    // The character keys: the rows from ` to / and the extra ones of ISO and JIS.
    QList<int> scans;
    for (int scan = 0x02; scan <= 0x0D; ++scan) scans << scan;
    for (int scan = 0x10; scan <= 0x1B; ++scan) scans << scan;
    for (int scan = 0x1E; scan <= 0x29; ++scan) scans << scan;
    for (int scan = 0x2B; scan <= 0x35; ++scan) scans << scan;
    scans << 0x56 << 0x73;
    for (int scan : std::as_const(scans)) {
        const QString typed = labelOf(scan, hkl);
        if (!typed.isEmpty())
            layout.labels.insert(scan, typed);
    }

    // A Japanese keyboard (type 7) is JIS, whatever the layout: the Japanese
    // layout on another keyboard types as the US one. ISO keyboards have a key
    // left of Z typing something other than the key above Enter; on ANSI
    // ones both are a backslash.
    if (GetKeyboardType(0) == 7)
        layout.physical = KeyboardLayout::Physical::Jis;
    else if (layout.labels.contains(0x56) && layout.labels.value(0x56) != layout.labels.value(0x2B))
        layout.physical = KeyboardLayout::Physical::Iso;
    known.insert(id, layout);
    return layout;
}

} // namespace

KeyboardLayout currentKeyboardLayout()
{
    DWORD thread = 0;
    if (const HWND window = GetForegroundWindow())
        thread = GetWindowThreadProcessId(window, nullptr);
    HKL hkl = GetKeyboardLayout(thread);
    if (!hkl)
        hkl = GetKeyboardLayout(0);
    return layoutOf(hkl);
}

KeyboardLayout keyboardLayoutNamed(const QString &name)
{
    const HKL hkl = LoadKeyboardLayoutW(reinterpret_cast<LPCWSTR>(name.utf16()), KLF_NOTELLSHELL);
    return layoutOf(hkl ? hkl : GetKeyboardLayout(0));
}

InputMonitor *InputMonitor::create(QObject *parent)
{
    return new WinInputMonitor(parent);
}
