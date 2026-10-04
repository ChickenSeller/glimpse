#pragma once

#include "CaptureMode.h"

#include <QColor>
#include <QKeySequence>
#include <QList>
#include <QString>
#include <QStringList>

// Typed access to the persisted settings (QSettings: registry on Windows,
// ~/.config/Glimpse/Glimpse.conf on Linux).
namespace AppSettings {

QKeySequence defaultHotkey(CaptureMode mode);
// The configured global hotkey; empty when the user disabled it.
QKeySequence hotkey(CaptureMode mode);
void setHotkey(CaptureMode mode, const QKeySequence &key);

// UI language code (see Language); empty follows the system language.
QString language();
void setLanguage(const QString &code);

// Whether captures show Glimpse's own toolbar and the mouse pointer.
bool captureIncludesToolbar();
void setCaptureIncludesToolbar(bool include);
bool captureIncludesCursor();
void setCaptureIncludesCursor(bool include);
// What fills a freehand capture outside the drawn outline: transparent by
// default, or a solid color (kept while transparency is switched on).
bool freehandTransparent();
void setFreehandTransparent(bool transparent);
QColor freehandColor();
void setFreehandColor(const QColor &color);
// The fill actually used: Qt::transparent or the color.
QColor freehandFill();

// Screen recording: what is painted into the video, its frame rate (one of
// recordFrameRates()) and where videos are saved.
bool recordHighlightCursor();
void setRecordHighlightCursor(bool on);
bool recordShowClicks();
void setRecordShowClicks(bool on);
bool recordShowKeys();
void setRecordShowKeys(bool on);
// 0: key labels, 1: full keyboard, 2: both (ScreenRecorder::KeyStyle order).
int recordKeyStyle();
void setRecordKeyStyle(int style);
QList<int> recordFrameRates();
int recordFrameRate();
void setRecordFrameRate(int rate);
QString defaultRecordFolder();
QString recordFolder();
void setRecordFolder(const QString &folder);

// Screenshot translation: engine (see Translate::engineIds()), target
// language, and the user's API key per engine (stored in plain text in the
// user's settings).
QString translateEngine();
void setTranslateEngine(const QString &engine);
QString translateTarget();
void setTranslateTarget(const QString &target);
QString translateKey(const QString &engine);
void setTranslateKey(const QString &engine, const QString &key);
// Local translation: a LocalModel preset id or "custom" (then the file).
QString localModel();
void setLocalModel(const QString &model);
QString localModelFile();
void setLocalModelFile(const QString &path);
QString microsoftTranslatorRegion();
void setMicrosoftTranslatorRegion(const QString &region);

// Seconds to wait before every capture (0 = none), set from the toolbar.
int captureDelay();
void setCaptureDelay(int seconds);

// Text recognition: engine id (see Ocr::engineIds()) and language codes.
QString ocrEngine();
void setOcrEngine(const QString &engine);
QStringList ocrLanguages();
void setOcrLanguages(const QStringList &languages);

} // namespace AppSettings
