#pragma once

#include "app/AppSettings.h"
#include "app/CaptureMode.h"

#include <QColor>
#include <QDialog>
#include <QHash>

#include <memory>

class HotkeyEdit;

namespace Ui {
class SettingsDialog;
}

// Application settings. Layout lives in SettingsDialog.ui; values are read on
// construction and written to AppSettings by Apply or OK.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override;

    // Shows beside each hotkey whether it works: `registered` holds the modes
    // whose hotkey was registered (true) or is taken by another program (false).
    void setHotkeyStatus(const QHash<CaptureMode, bool> &registered);

    void accept() override;

signals:
    // Settings were saved; the language has already been switched.
    void applied();
    // The user asked to check for an update right away.
    void updateNowRequested();

protected:
    void changeEvent(QEvent *event) override;

private:
    // Validates and saves; false (with the problem shown) if nothing was saved.
    bool apply();
    void setModified(bool modified);
    HotkeyEdit *hotkeyEdit(CaptureMode mode) const;
    void restoreDefaults();
    void clearSavedFiles(bool screenshots, bool recordings);
    QString selectedLanguage() const;
    QStringList selectedOcrLanguages() const;
    void setOcrLanguages(const QStringList &languages);
    void updateOcrEngineNote();
    void applyPlatformLimits();
    QString systemDefaultLabel() const;
    void setFreehandColor(const QColor &color);
    void showTranslateEngine();
    void setToolbarItems(const QList<AppSettings::ToolbarItem> &items);
    QList<AppSettings::ToolbarItem> toolbarItems() const;
    void updateToolbarItemTexts();
    void moveToolbarItem(int delta);

    std::unique_ptr<Ui::SettingsDialog> ui;
    bool m_modified = false;
    QColor m_freehandColor;
    // API keys as edited, per engine (the key field shows the current engine's).
    QHash<QString, QString> m_translateKeys;
    QString m_translateKeyEngine;
};
