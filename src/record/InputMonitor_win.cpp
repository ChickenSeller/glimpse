#include "InputMonitor.h"

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
            const int vk = int(key->vkCode);
            QMetaObject::invokeMethod(g_monitor, [vk, down] {
                if (g_monitor)
                    emit g_monitor->keyState(vk, down);
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

InputMonitor *InputMonitor::create(QObject *parent)
{
    return new WinInputMonitor(parent);
}
