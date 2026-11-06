#include "AppSettings.h"
#include "SettingsStore.h"

#include "ocr/OcrEngine.h"

#include <QCoreApplication>
#include <QDir>

#include "translate/LocalModel.h"
#include "translate/Translator.h"
#include <QStandardPaths>

#include <algorithm>

namespace {

QString hotkeySettingsKey(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Window: return QStringLiteral("hotkeys/window");
    case CaptureMode::FullScreen: return QStringLiteral("hotkeys/fullScreen");
    case CaptureMode::QrCode: return QStringLiteral("hotkeys/qrCode");
    case CaptureMode::Ocr: return QStringLiteral("hotkeys/ocr");
    case CaptureMode::Scrolling: return QStringLiteral("hotkeys/scrolling");
    case CaptureMode::Freehand: return QStringLiteral("hotkeys/freehand");
    case CaptureMode::Crosshair: return QStringLiteral("hotkeys/crosshair");
    case CaptureMode::Recording: return QStringLiteral("hotkeys/recording");
    case CaptureMode::Pin: return QStringLiteral("hotkeys/pin");
    case CaptureMode::Ruler: return QStringLiteral("hotkeys/ruler");
    case CaptureMode::Whiteboard: return QStringLiteral("hotkeys/whiteboard");
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
    case CaptureMode::FullScreen: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_3);
    case CaptureMode::QrCode: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_4);
    case CaptureMode::Ocr: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_5);
    case CaptureMode::Freehand: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_6);
    case CaptureMode::Crosshair: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_8);
    // V for video; Ctrl+Alt+R is taken by the NVIDIA overlay (performance overlay visibility).
    case CaptureMode::Recording: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_V);
    case CaptureMode::Scrolling: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_9);
    case CaptureMode::Pin: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_P); // P for pin
    case CaptureMode::Ruler: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_7); // beside the crosshair
    // Not W: WeChat has Ctrl+Alt+W.
    case CaptureMode::Whiteboard: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_2);
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
    // Full screen is a hotkey's job more than a button's.
    for (CaptureMode mode : kAllCaptureModes)
        items.append({toolbarItemId(mode), mode != CaptureMode::FullScreen});
    items.append({kDelayItemId, true});
    return items;
}

QList<ToolbarItem> toolbarItems()
{
    const QSettings settings = SettingsStore::open();
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
            items.append({item.id, item.visible && !hidden.contains(item.id)});
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
    QSettings settings = SettingsStore::open();
    settings.setValue(QStringLiteral("toolbar/order"), order);
    settings.setValue(QStringLiteral("toolbar/hidden"), hidden);
}

int toolbarScale()
{
    const QSettings settings = SettingsStore::open();
    int percent = settings.value(QStringLiteral("toolbar/scale"), 0).toInt();
    if (percent == 0) // not set: an icon size chosen by Glimpse 0.2 to 0.3, or the default
        percent = qRound(settings.value(QStringLiteral("toolbar/iconSize"), 24).toInt() * 100.0 / 24);
    return std::clamp(percent, kToolbarScaleMin, kToolbarScaleMax);
}

void setToolbarScale(int percent)
{
    QSettings settings = SettingsStore::open();
    settings.setValue(QStringLiteral("toolbar/scale"), std::clamp(percent, kToolbarScaleMin, kToolbarScaleMax));
    settings.remove(QStringLiteral("toolbar/iconSize"));
}

int toolbarIconSize()
{
    return qRound(24 * toolbarScale() / 100.0);
}

QKeySequence hotkey(CaptureMode mode)
{
    // Stored as portable text; a missing key means "default", an empty string "disabled".
    const QVariant value = SettingsStore::open().value(hotkeySettingsKey(mode));
    if (!value.isValid())
        return defaultHotkey(mode);
    return QKeySequence::fromString(value.toString(), QKeySequence::PortableText);
}

void setHotkey(CaptureMode mode, const QKeySequence &key)
{
    // Only deviations are stored, so a key left at its default follows future
    // changes of the default.
    QSettings settings = SettingsStore::open();
    if (key == defaultHotkey(mode))
        settings.remove(hotkeySettingsKey(mode));
    else
        settings.setValue(hotkeySettingsKey(mode), key.toString(QKeySequence::PortableText));
}

QString language()
{
    return SettingsStore::open().value(QStringLiteral("ui/language")).toString();
}

void setLanguage(const QString &code)
{
    SettingsStore::open().setValue(QStringLiteral("ui/language"), code);
}

bool captureIncludesToolbar()
{
    return SettingsStore::open().value(QStringLiteral("capture/includeToolbar"), false).toBool();
}

void setCaptureIncludesToolbar(bool include)
{
    SettingsStore::open().setValue(QStringLiteral("capture/includeToolbar"), include);
}

bool captureIncludesCursor()
{
    return SettingsStore::open().value(QStringLiteral("capture/includeCursor"), false).toBool();
}

void setCaptureIncludesCursor(bool include)
{
    SettingsStore::open().setValue(QStringLiteral("capture/includeCursor"), include);
}

bool copyCapturesToClipboard()
{
    return SettingsStore::open().value(QStringLiteral("capture/copyToClipboard"), false).toBool();
}

void setCopyCapturesToClipboard(bool copy)
{
    SettingsStore::open().setValue(QStringLiteral("capture/copyToClipboard"), copy);
}

bool adjustSelection()
{
    return SettingsStore::open().value(QStringLiteral("capture/adjustSelection"), false).toBool();
}

void setAdjustSelection(bool adjust)
{
    SettingsStore::open().setValue(QStringLiteral("capture/adjustSelection"), adjust);
}

bool scrollPreview()
{
    return SettingsStore::open().value(QStringLiteral("capture/scrollPreview"), true).toBool();
}

void setScrollPreview(bool show)
{
    SettingsStore::open().setValue(QStringLiteral("capture/scrollPreview"), show);
}

int scrollPreviewOpacity()
{
    const int percent = SettingsStore::open().value(QStringLiteral("capture/scrollPreviewOpacity"), 70).toInt();
    return std::clamp(percent, 20, 100);
}

void setScrollPreviewOpacity(int percent)
{
    SettingsStore::open().setValue(QStringLiteral("capture/scrollPreviewOpacity"), std::clamp(percent, 20, 100));
}

bool freehandTransparent()
{
    return SettingsStore::open().value(QStringLiteral("capture/freehandTransparent"), true).toBool();
}

void setFreehandTransparent(bool transparent)
{
    SettingsStore::open().setValue(QStringLiteral("capture/freehandTransparent"), transparent);
}

QColor freehandColor()
{
    const QColor color(SettingsStore::open().value(QStringLiteral("capture/freehandColor")).toString());
    return color.isValid() ? color : QColor(Qt::white);
}

void setFreehandColor(const QColor &color)
{
    SettingsStore::open().setValue(QStringLiteral("capture/freehandColor"), color.name(QColor::HexRgb));
}

QColor freehandFill()
{
    return freehandTransparent() ? QColor(Qt::transparent) : freehandColor();
}

bool recordHighlightCursor()
{
    return SettingsStore::open().value(QStringLiteral("recording/highlightCursor"), true).toBool();
}

void setRecordHighlightCursor(bool on)
{
    SettingsStore::open().setValue(QStringLiteral("recording/highlightCursor"), on);
}

bool recordShowClicks()
{
    return SettingsStore::open().value(QStringLiteral("recording/showClicks"), true).toBool();
}

void setRecordShowClicks(bool on)
{
    SettingsStore::open().setValue(QStringLiteral("recording/showClicks"), on);
}

bool recordShowKeys()
{
    return SettingsStore::open().value(QStringLiteral("recording/showKeys"), false).toBool();
}

void setRecordShowKeys(bool on)
{
    SettingsStore::open().setValue(QStringLiteral("recording/showKeys"), on);
}

int recordKeyStyle()
{
    return std::clamp(SettingsStore::open().value(QStringLiteral("recording/keyStyle"), 1).toInt(), 0, 2);
}

void setRecordKeyStyle(int style)
{
    SettingsStore::open().setValue(QStringLiteral("recording/keyStyle"), style);
}

int recordKeyPosition()
{
    const int position = SettingsStore::open().value(QStringLiteral("recording/keyPosition"), 0).toInt();
    constexpr int kPositions = 13; // ScreenRecorder::kKeyPositionCount, without pulling in Qt Multimedia
    return position >= 0 && position < kPositions ? position : 0;
}

void setRecordKeyPosition(int position)
{
    SettingsStore::open().setValue(QStringLiteral("recording/keyPosition"), position);
}

QList<int> recordFrameRates()
{
    return {15, 24, 30, 60};
}

int recordFrameRate()
{
    const int rate = SettingsStore::open().value(QStringLiteral("recording/frameRate"), 30).toInt();
    return recordFrameRates().contains(rate) ? rate : 30;
}

void setRecordFrameRate(int rate)
{
    SettingsStore::open().setValue(QStringLiteral("recording/frameRate"), rate);
}

bool recordFullColor()
{
    return SettingsStore::open().value(QStringLiteral("recording/fullColor"), false).toBool();
}

void setRecordFullColor(bool on)
{
    SettingsStore::open().setValue(QStringLiteral("recording/fullColor"), on);
}

QString defaultSaveFolder()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("output"));
}

QString saveFolder()
{
    QSettings settings = SettingsStore::open();
    QString folder = settings.value(QStringLiteral("general/saveFolder")).toString();
    if (folder.isEmpty()) // chosen before there was one folder for everything
        folder = settings.value(QStringLiteral("captures/saveFolder")).toString();
    return folder.isEmpty() ? defaultSaveFolder() : folder;
}

void setSaveFolder(const QString &folder)
{
    QSettings settings = SettingsStore::open();
    settings.remove(QStringLiteral("captures/saveFolder"));
    settings.remove(QStringLiteral("recording/folder"));
    // Only a chosen folder is stored, so the default follows the system's.
    if (folder.isEmpty() || QDir::cleanPath(folder) == QDir::cleanPath(defaultSaveFolder()))
        settings.remove(QStringLiteral("general/saveFolder"));
    else
        settings.setValue(QStringLiteral("general/saveFolder"), folder);
}

QString screenshotFolder(const QString &base)
{
    return QDir(base).filePath(QStringLiteral("Screenshots"));
}

QString recordFolder(const QString &base)
{
    return QDir(base).filePath(QStringLiteral("Recordings"));
}

QString translateEngine()
{
    return SettingsStore::open().value(QStringLiteral("translate/engine"), Translate::defaultEngine()).toString();
}

void setTranslateEngine(const QString &engine)
{
    SettingsStore::open().setValue(QStringLiteral("translate/engine"), engine);
}

QString translateTarget()
{
    const QString target = SettingsStore::open().value(QStringLiteral("translate/target")).toString();
    return Translate::targetLanguages().contains(target) ? target : Translate::defaultTarget();
}

void setTranslateTarget(const QString &target)
{
    SettingsStore::open().setValue(QStringLiteral("translate/target"), target);
}

QString translateKey(const QString &engine)
{
    return SettingsStore::open().value(QStringLiteral("translate/keys/") + engine).toString();
}

void setTranslateKey(const QString &engine, const QString &key)
{
    if (key.isEmpty())
        SettingsStore::open().remove(QStringLiteral("translate/keys/") + engine);
    else
        SettingsStore::open().setValue(QStringLiteral("translate/keys/") + engine, key);
}

QString localModel()
{
    return SettingsStore::open().value(QStringLiteral("translate/localModel"), LocalModel::defaultPreset()).toString();
}

void setLocalModel(const QString &model)
{
    SettingsStore::open().setValue(QStringLiteral("translate/localModel"), model);
}

QString localModelFile()
{
    return SettingsStore::open().value(QStringLiteral("translate/localModelFile")).toString();
}

void setLocalModelFile(const QString &path)
{
    SettingsStore::open().setValue(QStringLiteral("translate/localModelFile"), path);
}

QString microsoftTranslatorRegion()
{
    return SettingsStore::open().value(QStringLiteral("translate/microsoftRegion")).toString();
}

void setMicrosoftTranslatorRegion(const QString &region)
{
    SettingsStore::open().setValue(QStringLiteral("translate/microsoftRegion"), region);
}

int captureDelay()
{
    return std::clamp(SettingsStore::open().value(QStringLiteral("capture/delay"), 0).toInt(), 0, 60);
}

void setCaptureDelay(int seconds)
{
    SettingsStore::open().setValue(QStringLiteral("capture/delay"), std::clamp(seconds, 0, 60));
}

QString ocrEngine()
{
    // A stored engine may be missing from this build; fall back to the default.
    const QString engine = SettingsStore::open().value(QStringLiteral("ocr/engine")).toString();
    return Ocr::engineIds().contains(engine) ? engine : Ocr::defaultEngine();
}

void setOcrEngine(const QString &engine)
{
    SettingsStore::open().setValue(QStringLiteral("ocr/engine"), engine);
}

QStringList ocrLanguages()
{
    const QVariant value = SettingsStore::open().value(QStringLiteral("ocr/languages"));
    if (!value.isValid())
        return Ocr::defaultLanguages();
    // Only languages still offered.
    QStringList languages = value.toStringList();
    const QStringList supported = Ocr::supportedLanguages();
    languages.removeIf([&supported](const QString &language) { return !supported.contains(language); });
    return languages.isEmpty() ? Ocr::defaultLanguages() : languages;
}

void setOcrLanguages(const QStringList &languages)
{
    SettingsStore::open().setValue(QStringLiteral("ocr/languages"), languages);
}

} // namespace AppSettings
