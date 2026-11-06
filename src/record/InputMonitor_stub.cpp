#include "InputMonitor.h"

namespace {

class StubInputMonitor : public InputMonitor
{
public:
    using InputMonitor::InputMonitor;
    bool isSupported() const override { return false; }
    void start() override {}
    void stop() override {}
};

} // namespace

KeyboardLayout currentKeyboardLayout()
{
    return {};
}

KeyboardLayout keyboardLayoutNamed(const QString &)
{
    return {};
}

InputMonitor *InputMonitor::create(QObject *parent)
{
    return new StubInputMonitor(parent);
}
