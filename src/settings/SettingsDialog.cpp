#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"

#include "HotkeyEdit.h"
#include "app/AppSettings.h"
#include "app/Language.h"
#include "capture/Platform.h"
#include "ocr/OcrEngine.h"

#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::SettingsDialog>())
{
    ui->setupUi(this);

    // Language names are shown in their own language, so they are not translated.
    ui->languageCombo->addItem(systemDefaultLabel(), QString());
    const QStringList languages = Language::supported();
    for (const QString &code : languages)
        ui->languageCombo->addItem(Language::nativeName(code), code);
    ui->languageCombo->setCurrentIndex(std::max(0, ui->languageCombo->findData(AppSettings::language())));

    ui->captureToolbarCheck->setChecked(AppSettings::captureIncludesToolbar());
    ui->captureCursorCheck->setChecked(AppSettings::captureIncludesCursor());

    for (CaptureMode mode : kAllCaptureModes)
        hotkeyEdit(mode)->setKeySequence(AppSettings::hotkey(mode));

    // All engines are listed; those missing here are shown but cannot be picked.
    const QStringList engines = Ocr::allEngineIds();
    auto *engineModel = qobject_cast<QStandardItemModel *>(ui->ocrEngineCombo->model());
    for (const QString &engine : engines) {
        ui->ocrEngineCombo->addItem(Ocr::engineName(engine), engine);
        if (!Ocr::engineUnavailableReason(engine).isEmpty() && engineModel)
            engineModel->item(ui->ocrEngineCombo->count() - 1)->setEnabled(false);
    }
    ui->ocrEngineCombo->setEnabled(!Ocr::engineIds().isEmpty());
    ui->ocrEngineCombo->setCurrentIndex(std::max(0, ui->ocrEngineCombo->findData(AppSettings::ocrEngine())));
    setOcrLanguages(AppSettings::ocrLanguages());

    connect(ui->buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
            &SettingsDialog::restoreDefaults);
    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
            &SettingsDialog::apply);

    // Apply is only enabled while there is something to apply.
    connect(ui->languageCombo, &QComboBox::currentIndexChanged, this, [this] { setModified(true); });
    for (CaptureMode mode : kAllCaptureModes)
        connect(hotkeyEdit(mode), &QKeySequenceEdit::keySequenceChanged, this, [this] { setModified(true); });
    connect(ui->ocrEngineCombo, &QComboBox::currentIndexChanged, this, [this] {
        updateOcrEngineNote();
        setModified(true);
    });
    applyPlatformLimits();
    updateOcrEngineNote();
    for (QCheckBox *check : {ui->captureToolbarCheck, ui->captureCursorCheck, ui->ocrChineseCheck,
                             ui->ocrJapaneseCheck, ui->ocrEnglishCheck})
        connect(check, &QCheckBox::toggled, this, [this] { setModified(true); });
    setModified(false);
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::accept()
{
    if (m_modified && !apply())
        return;
    QDialog::accept();
}

void SettingsDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        ui->languageCombo->setItemText(0, systemDefaultLabel());
        applyPlatformLimits();
        updateOcrEngineNote();
        // The edits render key names (e.g. "Ctrl") in the UI language.
        for (CaptureMode mode : kAllCaptureModes) {
            HotkeyEdit *edit = hotkeyEdit(mode);
            const QSignalBlocker blocker(edit);
            edit->setKeySequence(edit->keySequence());
        }
    }
    QDialog::changeEvent(event);
}

bool SettingsDialog::apply()
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
            return false;
        }
        seen.insert(key, mode);
    }

    const QStringList ocrLanguages = selectedOcrLanguages();
    if (ocrLanguages.isEmpty()) {
        ui->tabs->setCurrentWidget(ui->ocrTab);
        QMessageBox::warning(this, windowTitle(), tr("Select at least one language for text recognition."));
        return false;
    }

    for (CaptureMode mode : kAllCaptureModes)
        AppSettings::setHotkey(mode, hotkeyEdit(mode)->keySequence());
    if (ui->ocrEngineCombo->count() > 0)
        AppSettings::setOcrEngine(ui->ocrEngineCombo->currentData().toString());
    AppSettings::setOcrLanguages(ocrLanguages);
    AppSettings::setCaptureIncludesToolbar(ui->captureToolbarCheck->isChecked());
    AppSettings::setCaptureIncludesCursor(ui->captureCursorCheck->isChecked());

    const QString language = selectedLanguage();
    if (language != AppSettings::language()) {
        AppSettings::setLanguage(language);
        Language::apply(language);
    }

    setModified(false);
    emit applied();
    return true;
}

void SettingsDialog::setModified(bool modified)
{
    m_modified = modified;
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(modified);
}

HotkeyEdit *SettingsDialog::hotkeyEdit(CaptureMode mode) const
{
    switch (mode) {
    case CaptureMode::Window: return ui->windowHotkeyEdit;
    case CaptureMode::Region: return ui->regionHotkeyEdit;
    case CaptureMode::FullScreen: return ui->fullScreenHotkeyEdit;
    case CaptureMode::QrCode: return ui->qrHotkeyEdit;
    case CaptureMode::Ocr: return ui->ocrHotkeyEdit;
    case CaptureMode::Scrolling: return ui->scrollHotkeyEdit;
    case CaptureMode::Freehand: return ui->freehandHotkeyEdit;
    }
    return nullptr;
}

QString SettingsDialog::selectedLanguage() const
{
    return ui->languageCombo->currentData().toString();
}

QString SettingsDialog::systemDefaultLabel() const
{
    return tr("System default (%1)").arg(Language::nativeName(Language::systemLanguage()));
}

void SettingsDialog::updateOcrEngineNote()
{
    const QString engine = ui->ocrEngineCombo->currentData().toString();
    const QString reason = Ocr::engineUnavailableReason(engine);
    ui->ocrEngineNote->setText(reason.isEmpty() ? Ocr::engineDescription(engine)
                                                : Ocr::engineDescription(engine) + QLatin1Char(' ') + reason);
}

void SettingsDialog::applyPlatformLimits()
{
    if (kScrollingHidden) {
        ui->scrollHotkeyLabel->hide();
        ui->scrollHotkeyEdit->hide();
    }

    // Features this platform lacks stay visible but disabled, with the reason.
    if (!Platform::supportsCursorCapture()) {
        ui->captureCursorCheck->setEnabled(false);
        ui->captureCursorCheck->setToolTip(Platform::unsupportedHint());
    }
    if (!Platform::supportsGlobalHotkeys()) {
        for (CaptureMode mode : kAllCaptureModes)
            hotkeyEdit(mode)->setEnabled(false);
        ui->hotkeysHint->setText(tr("Global hotkeys: %1").arg(Platform::unsupportedHint()));
    }

    auto *engineModel = qobject_cast<QStandardItemModel *>(ui->ocrEngineCombo->model());
    for (int i = 0; engineModel && i < ui->ocrEngineCombo->count(); ++i)
        engineModel->item(i)->setToolTip(Ocr::engineUnavailableReason(ui->ocrEngineCombo->itemData(i).toString()));
}

QStringList SettingsDialog::selectedOcrLanguages() const
{
    QStringList languages;
    if (ui->ocrChineseCheck->isChecked())
        languages << QStringLiteral("zh_CN");
    if (ui->ocrJapaneseCheck->isChecked())
        languages << QStringLiteral("ja");
    if (ui->ocrEnglishCheck->isChecked())
        languages << QStringLiteral("en");
    return languages;
}

void SettingsDialog::setOcrLanguages(const QStringList &languages)
{
    ui->ocrChineseCheck->setChecked(languages.contains(QLatin1String("zh_CN")));
    ui->ocrJapaneseCheck->setChecked(languages.contains(QLatin1String("ja")));
    ui->ocrEnglishCheck->setChecked(languages.contains(QLatin1String("en")));
}

void SettingsDialog::restoreDefaults()
{
    ui->languageCombo->setCurrentIndex(0); // system default
    ui->captureToolbarCheck->setChecked(false);
    ui->captureCursorCheck->setChecked(false);
    ui->ocrEngineCombo->setCurrentIndex(std::max(0, ui->ocrEngineCombo->findData(Ocr::defaultEngine())));
    setOcrLanguages(Ocr::defaultLanguages());
    for (CaptureMode mode : kAllCaptureModes)
        hotkeyEdit(mode)->setKeySequence(AppSettings::defaultHotkey(mode));
}
