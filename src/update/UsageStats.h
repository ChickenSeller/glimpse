#pragma once

#include <QString>

// Anonymous usage statistics, sent to Matomo (GLIMPSE_MATOMO_URL, site
// GLIMPSE_MATOMO_SITE_ID) through its HTTP tracking API: which versions are
// running, and how updating goes. Each install is a random ID made on first
// use; nothing about the user, their files or their screen is sent. On by
// default, can be turned off in Settings > General (which forgets the ID).
//
// Events (category / action / name):
//   App    / Start      / version          every start
//   App    / First run  / version          the first start of an install
//   Update / Mode       / update mode      every start
//   Update / Updated    / "old -> new"     the first start after the version changed
//   Update / Available  / version          a newer version was found at start
//   Update / Declined   / version          the user chose "Later"
//   Update / Failed     / version          downloading or starting the update failed
namespace UsageStats {

// False when the build has no Matomo site to send to.
bool isAvailable();
bool isEnabled();
void setEnabled(bool enabled);

// At start: App/Start, Update/Mode, and First run or Updated when they apply.
void reportStart(const QString &updateMode);
void report(const QString &category, const QString &action, const QString &name = QString());

} // namespace UsageStats
