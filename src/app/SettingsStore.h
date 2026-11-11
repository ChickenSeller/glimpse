#pragma once

#include <QSettings>

// Where the settings are kept: config.ini in the program's folder, where
// that can be written (a portable copy, a build), so a copy carries its
// settings along; elsewhere (installed read-only, as on Linux) the system's
// store: the registry on Windows, ~/.config/Glimpse/Glimpse.conf on Linux.
namespace SettingsStore {

// Takes config.ini when the program's folder can hold it, moving there the
// Glimpse/Glimpse.ini of Glimpse 0.7.1 and 0.7.2, or else carrying over the
// settings the system has kept so far. Also points kImageAnnotator's
// settings (the editor's tool colors, widths, fonts...) at the same file.
// Call once at start, after the application and organization names are set
// and before any setting is read.
void useProgramFolder();
// Keeps the settings in `file` instead, nothing carried over (for tools that
// want the defaults, see tests/SiteShots.cpp).
void useFile(const QString &file);

// The settings; what Glimpse uses in place of a default-constructed QSettings.
QSettings open();

} // namespace SettingsStore
