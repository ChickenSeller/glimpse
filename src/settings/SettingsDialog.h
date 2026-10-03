#pragma once

#include "app/CaptureMode.h"

#include <QDialog>

#include <memory>

class HotkeyEdit;

namespace Ui {
class SettingsDialog;
}

// Application settings. Layout lives in SettingsDialog.ui; values are read on
// construction and written to AppSettings when the user presses OK.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override;

    void accept() override;

private:
    HotkeyEdit *hotkeyEdit(CaptureMode mode) const;
    void restoreDefaults();
    QString selectedLanguage() const;

    std::unique_ptr<Ui::SettingsDialog> ui;
};
