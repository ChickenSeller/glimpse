#pragma once

#include <QString>

// Starting Glimpse when the user signs in. The state lives where the system
// keeps it, not in Glimpse's settings, so it is what will actually happen:
//   Windows  HKCU\Software\Microsoft\Windows\CurrentVersion\Run, value "Glimpse"
//   Linux    ~/.config/autostart/glimpse.desktop
// Glimpse is started with kArgument and then waits in the tray.
namespace Autostart {

inline const QString kArgument = QStringLiteral("--autostart");

bool isSupported();
bool isEnabled();
void setEnabled(bool enabled);
// At start: when enabled, points the entry at this copy of Glimpse (it may
// have been moved or unpacked somewhere else since).
void refresh();

} // namespace Autostart
