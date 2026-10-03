#include "AppSettings.h"

#include "ocr/OcrEngine.h"

#include <QSettings>

#include <algorithm>

namespace {

QString hotkeySettingsKey(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Window: return QStringLiteral("hotkeys/window");
    case CaptureMode::Region: return QStringLiteral("hotkeys/region");
    case CaptureMode::FullScreen: return QStringLiteral("hotkeys/fullScreen");
    case CaptureMode::QrCode: return QStringLiteral("hotkeys/qrCode");
    case CaptureMode::Ocr: return QStringLiteral("hotkeys/ocr");
    case CaptureMode::Scrolling: return QStringLiteral("hotkeys/scrolling");
    case CaptureMode::Freehand: return QStringLiteral("hotkeys/freehand");
    }
    return {};
}

} // namespace

namespace AppSettings {

// Ctrl+Alt+1..7: reachable with the left hand alone, and
// clear of common global hotkeys (WeChat Ctrl+Alt+W, QQ Ctrl+Alt+A/Z/O) and of
// the Ctrl+Shift+letter shortcuts applications use.
QKeySequence defaultHotkey(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Window: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_1);
    case CaptureMode::Region: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_2);
    case CaptureMode::FullScreen: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_3);
    case CaptureMode::QrCode: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_4);
    case CaptureMode::Ocr: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_5);
    case CaptureMode::Freehand: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_6);
    case CaptureMode::Scrolling: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_7);
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
    // Only deviations are stored, so a key left at its default follows future
    // changes of the default.
    QSettings settings;
    if (key == defaultHotkey(mode))
        settings.remove(hotkeySettingsKey(mode));
    else
        settings.setValue(hotkeySettingsKey(mode), key.toString(QKeySequence::PortableText));
}

QString language()
{
    return QSettings().value(QStringLiteral("ui/language")).toString();
}

void setLanguage(const QString &code)
{
    QSettings().setValue(QStringLiteral("ui/language"), code);
}

bool captureIncludesToolbar()
{
    return QSettings().value(QStringLiteral("capture/includeToolbar"), false).toBool();
}

void setCaptureIncludesToolbar(bool include)
{
    QSettings().setValue(QStringLiteral("capture/includeToolbar"), include);
}

bool captureIncludesCursor()
{
    return QSettings().value(QStringLiteral("capture/includeCursor"), false).toBool();
}

void setCaptureIncludesCursor(bool include)
{
    QSettings().setValue(QStringLiteral("capture/includeCursor"), include);
}

int captureDelay()
{
    return std::clamp(QSettings().value(QStringLiteral("capture/delay"), 0).toInt(), 0, 60);
}

void setCaptureDelay(int seconds)
{
    QSettings().setValue(QStringLiteral("capture/delay"), std::clamp(seconds, 0, 60));
}

QString ocrEngine()
{
    // A stored engine may be missing from this build; fall back to the default.
    const QString engine = QSettings().value(QStringLiteral("ocr/engine")).toString();
    return Ocr::engineIds().contains(engine) ? engine : Ocr::defaultEngine();
}

void setOcrEngine(const QString &engine)
{
    QSettings().setValue(QStringLiteral("ocr/engine"), engine);
}

QStringList ocrLanguages()
{
    const QVariant value = QSettings().value(QStringLiteral("ocr/languages"));
    return value.isValid() ? value.toStringList() : Ocr::defaultLanguages();
}

void setOcrLanguages(const QStringList &languages)
{
    QSettings().setValue(QStringLiteral("ocr/languages"), languages);
}

} // namespace AppSettings
