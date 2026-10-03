#pragma once

#include "CaptureMode.h"

#include <QColor>
#include <QKeySequence>
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

// Seconds to wait before every capture (0 = none), set from the toolbar.
int captureDelay();
void setCaptureDelay(int seconds);

// Text recognition: engine id (see Ocr::engineIds()) and language codes.
QString ocrEngine();
void setOcrEngine(const QString &engine);
QStringList ocrLanguages();
void setOcrLanguages(const QStringList &languages);

} // namespace AppSettings
