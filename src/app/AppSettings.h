#pragma once

#include "CaptureMode.h"

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

// Text recognition: engine id (see Ocr::engineIds()) and language codes.
QString ocrEngine();
void setOcrEngine(const QString &engine);
QStringList ocrLanguages();
void setOcrLanguages(const QStringList &languages);

} // namespace AppSettings
