#include "AppSettings.h"

#include "ocr/OcrEngine.h"

#include <QDir>
#include <QSettings>

#include "translate/LocalModel.h"
#include "translate/Translator.h"
#include <QStandardPaths>

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
    case CaptureMode::ColorPicker: return QStringLiteral("hotkeys/colorPicker");
    case CaptureMode::Crosshair: return QStringLiteral("hotkeys/crosshair");
    case CaptureMode::Recording: return QStringLiteral("hotkeys/recording");
    case CaptureMode::Translate: return QStringLiteral("hotkeys/translate");
    case CaptureMode::Pin: return QStringLiteral("hotkeys/pin");
    }
    return {};
}

} // namespace

namespace AppSettings {

// Ctrl+Alt+1..9: reachable with the left hand alone, and
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
    case CaptureMode::ColorPicker: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_7);
    case CaptureMode::Crosshair: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_8);
    // V for video; Ctrl+Alt+R is taken by the NVIDIA overlay (performance overlay visibility).
    case CaptureMode::Recording: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_V);
    case CaptureMode::Scrolling: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_9);
    case CaptureMode::Translate: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_T); // T for translate
    case CaptureMode::Pin: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_P); // P for pin
    }
    return {};
}

QString toolbarItemId(CaptureMode mode)
{
    return hotkeySettingsKey(mode).section(QLatin1Char('/'), 1); // e.g. "region"
}

QList<ToolbarItem> defaultToolbarItems()
{
    QList<ToolbarItem> items;
    for (CaptureMode mode : kAllCaptureModes)
        items.append({toolbarItemId(mode), true});
    items.append({kDelayItemId, true});
    return items;
}

QList<ToolbarItem> toolbarItems()
{
    const QSettings settings;
    const QStringList order = settings.value(QStringLiteral("toolbar/order")).toStringList();
    const QStringList hidden = settings.value(QStringLiteral("toolbar/hidden")).toStringList();
    const QList<ToolbarItem> defaults = defaultToolbarItems();
    const auto known = [&defaults](const QString &id) {
        return std::any_of(defaults.cbegin(), defaults.cend(), [&id](const ToolbarItem &item) { return item.id == id; });
    };

    QList<ToolbarItem> items;
    QStringList seen;
    for (const QString &id : order) {
        if (known(id) && !seen.contains(id)) {
            items.append({id, !hidden.contains(id)});
            seen << id;
        }
    }
    for (const ToolbarItem &item : defaults) {
        if (!seen.contains(item.id))
            items.append({item.id, !hidden.contains(item.id)});
    }
    return items;
}

void setToolbarItems(const QList<ToolbarItem> &items)
{
    QStringList order;
    QStringList hidden;
    for (const ToolbarItem &item : items) {
        order << item.id;
        if (!item.visible)
            hidden << item.id;
    }
    QSettings settings;
    settings.setValue(QStringLiteral("toolbar/order"), order);
    settings.setValue(QStringLiteral("toolbar/hidden"), hidden);
}

QList<int> toolbarIconSizes()
{
    return {20, 24, 32};
}

int toolbarIconSize()
{
    const int size = QSettings().value(QStringLiteral("toolbar/iconSize"), 24).toInt();
    return toolbarIconSizes().contains(size) ? size : 24;
}

void setToolbarIconSize(int size)
{
    QSettings().setValue(QStringLiteral("toolbar/iconSize"), size);
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

bool freehandTransparent()
{
    return QSettings().value(QStringLiteral("capture/freehandTransparent"), true).toBool();
}

void setFreehandTransparent(bool transparent)
{
    QSettings().setValue(QStringLiteral("capture/freehandTransparent"), transparent);
}

QColor freehandColor()
{
    const QColor color(QSettings().value(QStringLiteral("capture/freehandColor")).toString());
    return color.isValid() ? color : QColor(Qt::white);
}

void setFreehandColor(const QColor &color)
{
    QSettings().setValue(QStringLiteral("capture/freehandColor"), color.name(QColor::HexRgb));
}

QColor freehandFill()
{
    return freehandTransparent() ? QColor(Qt::transparent) : freehandColor();
}

bool recordHighlightCursor()
{
    return QSettings().value(QStringLiteral("recording/highlightCursor"), true).toBool();
}

void setRecordHighlightCursor(bool on)
{
    QSettings().setValue(QStringLiteral("recording/highlightCursor"), on);
}

bool recordShowClicks()
{
    return QSettings().value(QStringLiteral("recording/showClicks"), true).toBool();
}

void setRecordShowClicks(bool on)
{
    QSettings().setValue(QStringLiteral("recording/showClicks"), on);
}

bool recordShowKeys()
{
    return QSettings().value(QStringLiteral("recording/showKeys"), false).toBool();
}

void setRecordShowKeys(bool on)
{
    QSettings().setValue(QStringLiteral("recording/showKeys"), on);
}

int recordKeyStyle()
{
    return std::clamp(QSettings().value(QStringLiteral("recording/keyStyle"), 1).toInt(), 0, 2);
}

void setRecordKeyStyle(int style)
{
    QSettings().setValue(QStringLiteral("recording/keyStyle"), style);
}

QList<int> recordFrameRates()
{
    return {15, 24, 30, 60};
}

int recordFrameRate()
{
    const int rate = QSettings().value(QStringLiteral("recording/frameRate"), 30).toInt();
    return recordFrameRates().contains(rate) ? rate : 30;
}

void setRecordFrameRate(int rate)
{
    QSettings().setValue(QStringLiteral("recording/frameRate"), rate);
}

QString defaultRecordFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)).filePath(QStringLiteral("Glimpse"));
}

QString recordFolder()
{
    const QString folder = QSettings().value(QStringLiteral("recording/folder")).toString();
    return folder.isEmpty() ? defaultRecordFolder() : folder;
}

void setRecordFolder(const QString &folder)
{
    // Only a chosen folder is stored, so the default follows the system's.
    if (QDir::cleanPath(folder) == QDir::cleanPath(defaultRecordFolder()))
        QSettings().remove(QStringLiteral("recording/folder"));
    else
        QSettings().setValue(QStringLiteral("recording/folder"), folder);
}

QString translateEngine()
{
    return QSettings().value(QStringLiteral("translate/engine"), Translate::defaultEngine()).toString();
}

void setTranslateEngine(const QString &engine)
{
    QSettings().setValue(QStringLiteral("translate/engine"), engine);
}

QString translateTarget()
{
    const QString target = QSettings().value(QStringLiteral("translate/target")).toString();
    return Translate::targetLanguages().contains(target) ? target : Translate::defaultTarget();
}

void setTranslateTarget(const QString &target)
{
    QSettings().setValue(QStringLiteral("translate/target"), target);
}

QString translateKey(const QString &engine)
{
    return QSettings().value(QStringLiteral("translate/keys/") + engine).toString();
}

void setTranslateKey(const QString &engine, const QString &key)
{
    if (key.isEmpty())
        QSettings().remove(QStringLiteral("translate/keys/") + engine);
    else
        QSettings().setValue(QStringLiteral("translate/keys/") + engine, key);
}

QString localModel()
{
    return QSettings().value(QStringLiteral("translate/localModel"), LocalModel::defaultPreset()).toString();
}

void setLocalModel(const QString &model)
{
    QSettings().setValue(QStringLiteral("translate/localModel"), model);
}

QString localModelFile()
{
    return QSettings().value(QStringLiteral("translate/localModelFile")).toString();
}

void setLocalModelFile(const QString &path)
{
    QSettings().setValue(QStringLiteral("translate/localModelFile"), path);
}

QString microsoftTranslatorRegion()
{
    return QSettings().value(QStringLiteral("translate/microsoftRegion")).toString();
}

void setMicrosoftTranslatorRegion(const QString &region)
{
    QSettings().setValue(QStringLiteral("translate/microsoftRegion"), region);
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
