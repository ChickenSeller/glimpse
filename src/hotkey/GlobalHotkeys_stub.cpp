#include "GlobalHotkeys.h"

namespace {

// TODO: X11 via XGrabKey on the root window; Wayland via the
// org.freedesktop.portal.GlobalShortcuts portal.
class NullGlobalHotkeys : public GlobalHotkeys
{
public:
    using GlobalHotkeys::GlobalHotkeys;

    bool isSupported() const override { return false; }
    bool add(int, const QKeySequence &, const QString &) override { return false; }
    void clear() override {}
};

} // namespace

GlobalHotkeys *GlobalHotkeys::create(QObject *parent)
{
    return new NullGlobalHotkeys(parent);
}
