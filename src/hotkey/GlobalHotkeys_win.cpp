#include "GlobalHotkeys.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QList>

#include <windows.h>

namespace {

// RegisterHotKey ids for applications must lie in 0x0000-0xBFFF.
constexpr int kIdBase = 0x4700;

UINT toVirtualKey(Qt::Key key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return 'A' + (key - Qt::Key_A);
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return '0' + (key - Qt::Key_0);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return VK_F1 + (key - Qt::Key_F1);

    switch (key) {
    case Qt::Key_Print: return VK_SNAPSHOT;
    case Qt::Key_Pause: return VK_PAUSE;
    case Qt::Key_ScrollLock: return VK_SCROLL;
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Down: return VK_DOWN;
    default: return 0;
    }
}

UINT toModifiers(Qt::KeyboardModifiers modifiers)
{
    UINT result = MOD_NOREPEAT;
    if (modifiers & Qt::ShiftModifier)
        result |= MOD_SHIFT;
    if (modifiers & Qt::ControlModifier)
        result |= MOD_CONTROL;
    if (modifiers & Qt::AltModifier)
        result |= MOD_ALT;
    if (modifiers & Qt::MetaModifier)
        result |= MOD_WIN;
    return result;
}

// Hotkeys are registered on the GUI thread without a window, so WM_HOTKEY
// arrives as a thread message, which Qt passes to native event filters.
class WinGlobalHotkeys : public GlobalHotkeys, public QAbstractNativeEventFilter
{
public:
    explicit WinGlobalHotkeys(QObject *parent)
        : GlobalHotkeys(parent)
    {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }

    ~WinGlobalHotkeys() override
    {
        clear();
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }

    bool isSupported() const override { return true; }

    bool add(int id, const QKeySequence &key, const QString &) override
    {
        if (key.isEmpty())
            return false;
        const QKeyCombination combination = key[0];
        const UINT vk = toVirtualKey(combination.key());
        if (vk == 0)
            return false;
        if (!RegisterHotKey(nullptr, kIdBase + id, toModifiers(combination.keyboardModifiers()), vk))
            return false;
        m_ids.append(id);
        return true;
    }

    void clear() override
    {
        for (int id : std::as_const(m_ids))
            UnregisterHotKey(nullptr, kIdBase + id);
        m_ids.clear();
    }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) override
    {
        if (eventType != "windows_generic_MSG")
            return false;
        const MSG *msg = static_cast<const MSG *>(message);
        if (msg->message != WM_HOTKEY || msg->hwnd != nullptr)
            return false;
        const int id = int(msg->wParam) - kIdBase;
        if (!m_ids.contains(id))
            return false;
        emit activated(id);
        return true;
    }

private:
    QList<int> m_ids;
};

} // namespace

GlobalHotkeys *GlobalHotkeys::create(QObject *parent)
{
    return new WinGlobalHotkeys(parent);
}
