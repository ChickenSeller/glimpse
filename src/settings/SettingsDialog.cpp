#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"

#include "HotkeyEdit.h"
#include "app/AppSettings.h"
#include "app/Language.h"

#include <QMessageBox>
#include <QPushButton>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::SettingsDialog>())
{
    ui->setupUi(this);

    // Language names are shown in their own language, so they are not translated.
    ui->languageCombo->addItem(tr("System default (%1)").arg(Language::nativeName(Language::systemLanguage())),
                               QString());
    const QStringList languages = Language::supported();
    for (const QString &code : languages)
        ui->languageCombo->addItem(Language::nativeName(code), code);
    ui->languageCombo->setCurrentIndex(std::max(0, ui->languageCombo->findData(AppSettings::language())));

    for (CaptureMode mode : kAllCaptureModes)
        hotkeyEdit(mode)->setKeySequence(AppSettings::hotkey(mode));

    connect(ui->buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
            &SettingsDialog::restoreDefaults);
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::accept()
{
    // The same combination cannot trigger two actions.
    QHash<QKeySequence, CaptureMode> seen;
    for (CaptureMode mode : kAllCaptureModes) {
        HotkeyEdit *edit = hotkeyEdit(mode);
        const QKeySequence key = edit->keySequence();
        if (key.isEmpty())
            continue;
        if (seen.contains(key)) {
            ui->tabs->setCurrentWidget(ui->hotkeysTab);
            edit->setFocus();
            QMessageBox::warning(this, windowTitle(),
                                 tr("%1 is assigned to more than one action.")
                                     .arg(key.toString(QKeySequence::NativeText)));
            return;
        }
        seen.insert(key, mode);
    }

    for (CaptureMode mode : kAllCaptureModes)
        AppSettings::setHotkey(mode, hotkeyEdit(mode)->keySequence());

    const QString language = selectedLanguage();
    if (language != AppSettings::language()) {
        AppSettings::setLanguage(language);
        Language::apply(language);
    }
    QDialog::accept();
}

HotkeyEdit *SettingsDialog::hotkeyEdit(CaptureMode mode) const
{
    switch (mode) {
    case CaptureMode::Window: return ui->windowHotkeyEdit;
    case CaptureMode::Region: return ui->regionHotkeyEdit;
    case CaptureMode::FullScreen: return ui->fullScreenHotkeyEdit;
    }
    return nullptr;
}

QString SettingsDialog::selectedLanguage() const
{
    return ui->languageCombo->currentData().toString();
}

void SettingsDialog::restoreDefaults()
{
    ui->languageCombo->setCurrentIndex(0); // system default
    for (CaptureMode mode : kAllCaptureModes)
        hotkeyEdit(mode)->setKeySequence(AppSettings::defaultHotkey(mode));
}
