#pragma once

#include "CaptureMode.h"

#include <QKeySequence>
#include <QString>

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

} // namespace AppSettings
