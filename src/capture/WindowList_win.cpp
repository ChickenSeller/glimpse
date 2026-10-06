#include "WindowList.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QScreen>
#include <QtGui/qscreen_platform.h>

#include <windows.h>
#include <dwmapi.h>

namespace {

constexpr int kMaxDepth = 8;
constexpr int kMaxNodes = 20000;

// Win32 reports physical pixels; Qt positions screens in logical units with a
// per-monitor scale, so each rect is mapped through the monitor it sits on.
class CoordinateMapper
{
public:
    CoordinateMapper()
    {
        const auto screens = QGuiApplication::screens();
        for (QScreen *screen : screens) {
            auto *native = screen->nativeInterface<QNativeInterface::QWindowsScreen>();
            MONITORINFO info{};
            info.cbSize = sizeof(info);
            if (!native || !GetMonitorInfoW(native->handle(), &info))
                continue;
            const RECT &r = info.rcMonitor;
            const QRect nativeRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
            const QRect logical = screen->geometry();
            if (logical.width() > 0)
                m_monitors.append({nativeRect, logical, nativeRect.width() / qreal(logical.width())});
        }
    }

    QRect toLogical(const RECT &r) const
    {
        if (m_monitors.isEmpty())
            return QRect(r.left, r.top, r.right - r.left, r.bottom - r.top);

        const QPoint center((r.left + r.right) / 2, (r.top + r.bottom) / 2);
        const Monitor *best = &m_monitors.first();
        for (const Monitor &m : m_monitors) {
            if (m.native.contains(center)) {
                best = &m;
                break;
            }
        }

        const auto map = [best](int x, int y) {
            return QPointF(best->logical.x() + (x - best->native.x()) / best->scale,
                           best->logical.y() + (y - best->native.y()) / best->scale);
        };
        const QPointF topLeft = map(r.left, r.top);
        const QPointF bottomRight = map(r.right, r.bottom); // exclusive
        return QRect(QPoint(qRound(topLeft.x()), qRound(topLeft.y())),
                     QPoint(qRound(bottomRight.x()) - 1, qRound(bottomRight.y()) - 1));
    }

private:
    struct Monitor {
        QRect native;
        QRect logical;
        qreal scale;
    };
    QList<Monitor> m_monitors;
};

QString className(HWND hwnd)
{
    wchar_t buffer[256];
    const int length = GetClassNameW(hwnd, buffer, int(std::size(buffer)));
    return QString::fromWCharArray(buffer, length);
}

// GetWindowText does not send WM_GETTEXT across processes, so a hung app cannot block us.
QString windowTitle(HWND hwnd)
{
    wchar_t buffer[256];
    const int length = GetWindowTextW(hwnd, buffer, int(std::size(buffer)));
    return QString::fromWCharArray(buffer, qMax(length, 0));
}

// "notepad.exe"; empty when the process cannot be opened (elevated, gone).
QString processName(DWORD process)
{
    QString name;
    if (HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process)) {
        wchar_t buffer[MAX_PATH];
        DWORD size = DWORD(std::size(buffer));
        if (QueryFullProcessImageNameW(handle, 0, buffer, &size))
            name = QFileInfo(QString::fromWCharArray(buffer, int(size))).fileName();
        CloseHandle(handle);
    }
    return name;
}

bool isCloaked(HWND hwnd)
{
    DWORD cloaked = 0;
    return SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked;
}

bool isEmpty(const RECT &r)
{
    return r.right <= r.left || r.bottom <= r.top;
}

struct Context {
    CoordinateMapper mapper;
    DWORD ownProcess = GetCurrentProcessId();
    QHash<DWORD, QString> processNames;
    int budget = kMaxNodes;
    std::vector<WindowNode> *out = nullptr;
};

// "Title (app.exe) · ClassName": the window the control belongs to (a
// control's own title is mostly empty), and the control's class.
QString withClass(const QString &label, HWND hwnd)
{
    const QString cls = className(hwnd);
    if (label.isEmpty() || cls.isEmpty())
        return label.isEmpty() ? cls : label;
    return QStringLiteral("%1  ·  %2").arg(label, cls);
}

// Controls inside a window are labelled with the window's "Title (app.exe)".
void collectChildren(HWND parent, const QString &label, Context &ctx, std::vector<WindowNode> &out, int depth)
{
    if (depth > kMaxDepth)
        return;
    // GW_CHILD / GW_HWNDNEXT walks direct children in z-order, topmost first.
    for (HWND child = GetWindow(parent, GW_CHILD); child && ctx.budget > 0;
         child = GetWindow(child, GW_HWNDNEXT)) {
        RECT rect;
        if (!IsWindowVisible(child) || !GetWindowRect(child, &rect) || isEmpty(rect))
            continue;
        --ctx.budget;
        WindowNode node;
        node.geometry = ctx.mapper.toLogical(rect);
        node.title = withClass(label, child);
        collectChildren(child, label, ctx, node.children, depth + 1);
        out.push_back(std::move(node));
    }
}

BOOL CALLBACK collectTopLevel(HWND hwnd, LPARAM param)
{
    auto &ctx = *reinterpret_cast<Context *>(param);
    if (ctx.budget <= 0)
        return FALSE;

    if (!IsWindowVisible(hwnd) || IsIconic(hwnd) || isCloaked(hwnd))
        return TRUE;

    DWORD process = 0;
    GetWindowThreadProcessId(hwnd, &process);
    if (process == ctx.ownProcess)
        return TRUE;

    // Click-through overlays (game/GPU overlays, some notifications) are not real targets.
    if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT)
        return TRUE;

    // The desktop itself: an empty spot falls back to the monitor instead.
    const QString cls = className(hwnd);
    if (cls == QLatin1String("Progman") || cls == QLatin1String("WorkerW"))
        return TRUE;

    // The extended frame excludes the invisible resize borders of Windows 10/11.
    RECT rect;
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rect, sizeof(rect)))
        && !GetWindowRect(hwnd, &rect))
        return TRUE;
    if (isEmpty(rect))
        return TRUE;

    --ctx.budget;
    WindowNode node;
    node.geometry = ctx.mapper.toLogical(rect);
    // "Title (app.exe)", so that it is clear which program a window belongs to.
    if (!ctx.processNames.contains(process))
        ctx.processNames.insert(process, processName(process));
    const QString program = ctx.processNames.value(process);
    const QString title = windowTitle(hwnd);
    QString label = title;
    if (!program.isEmpty())
        label = title.isEmpty() ? program : QStringLiteral("%1 (%2)").arg(title, program);
    node.title = withClass(label, hwnd);
    collectChildren(hwnd, label, ctx, node.children, 1);
    ctx.out->push_back(std::move(node));
    return TRUE;
}

} // namespace

std::vector<WindowNode> enumerateWindows()
{
    std::vector<WindowNode> windows;
    Context ctx;
    ctx.out = &windows;
    // EnumWindows visits top-level windows in z-order, topmost first.
    EnumWindows(collectTopLevel, reinterpret_cast<LPARAM>(&ctx));
    return windows;
}
