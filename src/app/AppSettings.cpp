#include "AppSettings.h"

#include <QSettings>

namespace {

QString hotkeySettingsKey(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Window: return QStringLiteral("hotkeys/window");
    case CaptureMode::Region: return QStringLiteral("hotkeys/region");
    case CaptureMode::FullScreen: return QStringLiteral("hotkeys/fullScreen");
    }
    return {};
}

} // namespace

namespace AppSettings {

// Defaults follow FastStone Capture. Plain PrtSc is left alone: Windows 11
// gives it to the Snipping Tool by default.
QKeySequence defaultHotkey(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Window: return QKeySequence(Qt::SHIFT | Qt::Key_Print);
    case CaptureMode::Region: return QKeySequence(Qt::CTRL | Qt::Key_Print);
    case CaptureMode::FullScreen: return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Print);
    }
    return {};
}

QKeySequence hotkey(CaptureMode mode)
{
    // Stored as portable text; a missing key means "default", an empty string "disabled".
    const QVariant value = QSettings().value(hotkeySettingsKey(mode));
    if (!value.isValid())
        return defaultHotkey(mode);
    return QKeySequence::fromString(value.toString(), QKeySequence::PortableText);
}

void setHotkey(CaptureMode mode, const QKeySequence &key)
{
    QSettings().setValue(hotkeySettingsKey(mode), key.toString(QKeySequence::PortableText));
}

QString language()
{
    return QSettings().value(QStringLiteral("ui/language")).toString();
}

void setLanguage(const QString &code)
{
    QSettings().setValue(QStringLiteral("ui/language"), code);
}

} // namespace AppSettings
