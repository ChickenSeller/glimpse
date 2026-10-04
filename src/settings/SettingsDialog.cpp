#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"

#include "HotkeyEdit.h"
#include "app/AppSettings.h"
#include "app/Language.h"
#include "app/TintedIcon.h"
#include "capture/Platform.h"
#include "ocr/ModelStore.h"
#include "ocr/OcrEngine.h"
#include "translate/FirefoxTranslation.h"
#include "translate/LocalModel.h"
#include "translate/Translator.h"
#include "update/Updater.h"

#include <QColorDialog>
#include <QCoreApplication>
#include <QListWidget>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
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

    // Updates: the combo's rows are in Updater::Mode's order.
    ui->updateModeCombo->setCurrentIndex(int(Updater::mode()));
    connect(ui->updateModeCombo, &QComboBox::currentIndexChanged, this, [this] { setModified(true); });

    // Downloads: the combo's rows are in ModelStore::SourceOrder's order.
    ui->downloadOrderCombo->setCurrentIndex(int(ModelStore::sourceOrder()));
    ui->downloadMirrorEdit->setPlaceholderText(ModelStore::defaultMirror());
    ui->downloadMirrorEdit->setText(ModelStore::mirror());
    const auto updateMirror = [this] {
        ui->downloadMirrorEdit->setEnabled(ui->downloadOrderCombo->currentIndex()
                                           != int(ModelStore::SourceOrder::OfficialOnly));
    };
    updateMirror();
    connect(ui->downloadOrderCombo, &QComboBox::currentIndexChanged, this, [this, updateMirror] {
        updateMirror();
        setModified(true);
    });
    connect(ui->downloadMirrorEdit, &QLineEdit::textEdited, this, [this] { setModified(true); });

    ui->captureToolbarCheck->setChecked(AppSettings::captureIncludesToolbar());
    ui->captureCursorCheck->setChecked(AppSettings::captureIncludesCursor());
    ui->freehandTransparentCheck->setChecked(AppSettings::freehandTransparent());

    ui->recordHighlightCheck->setChecked(AppSettings::recordHighlightCursor());
    ui->recordClicksCheck->setChecked(AppSettings::recordShowClicks());
    ui->recordKeysCheck->setChecked(AppSettings::recordShowKeys());
    ui->recordKeyStyleCombo->setCurrentIndex(AppSettings::recordKeyStyle());
    // The style only matters while keys are shown.
    const auto updateKeyStyle = [this] {
        const bool on = ui->recordKeysCheck->isEnabled() && ui->recordKeysCheck->isChecked();
        ui->recordKeyStyleLabel->setEnabled(on);
        ui->recordKeyStyleCombo->setEnabled(on);
    };
    connect(ui->recordKeysCheck, &QCheckBox::toggled, this, updateKeyStyle);
    connect(ui->recordKeyStyleCombo, &QComboBox::currentIndexChanged, this, [this] { setModified(true); });
    QTimer::singleShot(0, this, updateKeyStyle); // after applyPlatformLimits()
    const QList<int> rates = AppSettings::recordFrameRates();
    for (int rate : rates)
        ui->recordFrameRateCombo->addItem(tr("%1 fps").arg(rate), rate);
    ui->recordFrameRateCombo->setCurrentIndex(
        std::max(0, ui->recordFrameRateCombo->findData(AppSettings::recordFrameRate())));
    ui->recordFolderEdit->setText(QDir::toNativeSeparators(AppSettings::recordFolder()));
    connect(ui->recordFolderButton, &QPushButton::clicked, this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(this, ui->recordFolderLabel->text().remove(QLatin1Char('&')),
                                                                 ui->recordFolderEdit->text());
        if (!folder.isEmpty())
            ui->recordFolderEdit->setText(QDir::toNativeSeparators(folder));
    });
    connect(ui->recordFrameRateCombo, &QComboBox::currentIndexChanged, this, [this] { setModified(true); });
    connect(ui->recordFolderEdit, &QLineEdit::textChanged, this, [this] { setModified(true); });
    setFreehandColor(AppSettings::freehandColor());

    for (CaptureMode mode : kAllCaptureModes)
        hotkeyEdit(mode)->setKeySequence(AppSettings::hotkey(mode));

    // Toolbar: its buttons as a checkable list, in toolbar order.
    setToolbarItems(AppSettings::toolbarItems());
    const QList<int> iconSizes = AppSettings::toolbarIconSizes();
    for (int size : iconSizes)
        ui->toolbarIconSizeCombo->addItem(QString(), size);
    updateToolbarItemTexts();
    ui->toolbarIconSizeCombo->setCurrentIndex(
        std::max(0, ui->toolbarIconSizeCombo->findData(AppSettings::toolbarIconSize())));
    connect(ui->toolbarUpButton, &QPushButton::clicked, this, [this] { moveToolbarItem(-1); });
    connect(ui->toolbarDownButton, &QPushButton::clicked, this, [this] { moveToolbarItem(1); });
    const auto updateMoveButtons = [this] {
        const int row = ui->toolbarList->currentRow();
        ui->toolbarUpButton->setEnabled(row > 0);
        ui->toolbarDownButton->setEnabled(row >= 0 && row < ui->toolbarList->count() - 1);
    };
    connect(ui->toolbarList, &QListWidget::currentRowChanged, this, updateMoveButtons);
    updateMoveButtons();
    connect(ui->toolbarList, &QListWidget::itemChanged, this, [this] { setModified(true); });
    // A drag within the list moves a row.
    connect(ui->toolbarList->model(), &QAbstractItemModel::rowsMoved, this, [this, updateMoveButtons] {
        updateMoveButtons();
        setModified(true);
    });
    connect(ui->toolbarIconSizeCombo, &QComboBox::currentIndexChanged, this, [this] { setModified(true); });

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

    // Translation: the key field edits the selected engine's key; the others
    // are kept here until Apply.
    const QStringList translators = Translate::engineIds();
    for (const QString &engine : translators) {
        ui->translateEngineCombo->addItem(Translate::engineName(engine), engine);
        m_translateKeys.insert(engine, AppSettings::translateKey(engine));
    }
    const QStringList targets = Translate::targetLanguages();
    for (const QString &code : targets)
        ui->translateTargetCombo->addItem(Translate::languageName(code), code);
    ui->translateTargetCombo->setCurrentIndex(std::max(0, ui->translateTargetCombo->findData(AppSettings::translateTarget())));
    ui->translateRegionEdit->setText(AppSettings::microsoftTranslatorRegion());
    const QList<LocalModel::Preset> models = LocalModel::presets();
    for (const LocalModel::Preset &model : models)
        ui->translateModelCombo->addItem(model.name, model.id);
    ui->translateModelCombo->addItem(tr("Other GGUF model file..."), QString::fromLatin1(LocalModel::kCustom));
    ui->translateModelCombo->setCurrentIndex(std::max(0, ui->translateModelCombo->findData(AppSettings::localModel())));
    ui->translateModelFileEdit->setText(QDir::toNativeSeparators(AppSettings::localModelFile()));
    connect(ui->translateModelCombo, &QComboBox::currentIndexChanged, this, [this] {
        showTranslateEngine();
        setModified(true);
    });
    connect(ui->translateModelFileEdit, &QLineEdit::textEdited, this, [this] { setModified(true); });
    connect(ui->translateModelFileButton, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(this, tr("Choose a GGUF Model"), ui->translateModelFileEdit->text(),
                                                          tr("GGUF models (*.gguf)"));
        if (!file.isEmpty()) {
            ui->translateModelFileEdit->setText(QDir::toNativeSeparators(file));
            setModified(true);
        }
    });
    ui->translateEngineCombo->setCurrentIndex(std::max(0, ui->translateEngineCombo->findData(AppSettings::translateEngine())));
    showTranslateEngine();
    connect(ui->translateEngineCombo, &QComboBox::currentIndexChanged, this, [this] {
        showTranslateEngine();
        setModified(true);
    });
    connect(ui->translateKeyEdit, &QLineEdit::textEdited, this, [this](const QString &key) {
        m_translateKeys.insert(m_translateKeyEngine, key.trimmed());
        setModified(true);
    });
    connect(ui->translateTargetCombo, &QComboBox::currentIndexChanged, this, [this] { setModified(true); });
    connect(ui->translateRegionEdit, &QLineEdit::textEdited, this, [this] { setModified(true); });

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
    // The color is kept while transparency is on, so switching back restores it.
    connect(ui->freehandTransparentCheck, &QCheckBox::toggled, ui->freehandColorButton,
            [this](bool transparent) { ui->freehandColorButton->setEnabled(!transparent); });
    ui->freehandColorButton->setEnabled(!ui->freehandTransparentCheck->isChecked());
    connect(ui->freehandColorButton, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(m_freehandColor, this, ui->freehandFillLabel->text());
        if (color.isValid() && color != m_freehandColor) {
            setFreehandColor(color);
            setModified(true);
        }
    });
    applyPlatformLimits();
    updateOcrEngineNote();
    for (QCheckBox *check : {ui->captureToolbarCheck, ui->captureCursorCheck, ui->freehandTransparentCheck,
                             ui->recordHighlightCheck, ui->recordClicksCheck, ui->recordKeysCheck,
                             ui->ocrChineseCheck, ui->ocrJapaneseCheck, ui->ocrEnglishCheck})
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
        for (int i = 0; i < ui->translateEngineCombo->count(); ++i)
            ui->translateEngineCombo->setItemText(i, Translate::engineName(ui->translateEngineCombo->itemData(i).toString()));
        showTranslateEngine();
        applyPlatformLimits();
        updateOcrEngineNote();
        updateToolbarItemTexts();
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
    AppSettings::setTranslateEngine(ui->translateEngineCombo->currentData().toString());
    AppSettings::setTranslateTarget(ui->translateTargetCombo->currentData().toString());
    for (auto it = m_translateKeys.cbegin(); it != m_translateKeys.cend(); ++it)
        AppSettings::setTranslateKey(it.key(), it.value());
    AppSettings::setMicrosoftTranslatorRegion(ui->translateRegionEdit->text().trimmed());
    AppSettings::setLocalModel(ui->translateModelCombo->currentData().toString());
    AppSettings::setLocalModelFile(QDir::fromNativeSeparators(ui->translateModelFileEdit->text().trimmed()));
    if (Updater::isSupported())
        Updater::setMode(Updater::Mode(ui->updateModeCombo->currentIndex()));
    ModelStore::setSourceOrder(ModelStore::SourceOrder(ui->downloadOrderCombo->currentIndex()));
    ModelStore::setMirror(ui->downloadMirrorEdit->text());
    AppSettings::setToolbarItems(toolbarItems());
    AppSettings::setToolbarIconSize(ui->toolbarIconSizeCombo->currentData().toInt());
    AppSettings::setCaptureIncludesToolbar(ui->captureToolbarCheck->isChecked());
    AppSettings::setCaptureIncludesCursor(ui->captureCursorCheck->isChecked());
    AppSettings::setFreehandTransparent(ui->freehandTransparentCheck->isChecked());
    AppSettings::setFreehandColor(m_freehandColor);
    AppSettings::setRecordHighlightCursor(ui->recordHighlightCheck->isChecked());
    AppSettings::setRecordShowClicks(ui->recordClicksCheck->isChecked());
    AppSettings::setRecordShowKeys(ui->recordKeysCheck->isChecked());
    AppSettings::setRecordKeyStyle(ui->recordKeyStyleCombo->currentIndex());
    AppSettings::setRecordFrameRate(ui->recordFrameRateCombo->currentData().toInt());
    const QString recordFolder = QDir::fromNativeSeparators(ui->recordFolderEdit->text().trimmed());
    AppSettings::setRecordFolder(recordFolder.isEmpty() ? AppSettings::defaultRecordFolder() : recordFolder);

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
    case CaptureMode::ColorPicker: return ui->colorHotkeyEdit;
    case CaptureMode::Crosshair: return ui->crosshairHotkeyEdit;
    case CaptureMode::Recording: return ui->recordHotkeyEdit;
    case CaptureMode::Translate: return ui->translateHotkeyEdit;
    case CaptureMode::Pin: return ui->pinHotkeyEdit;
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

void SettingsDialog::setFreehandColor(const QColor &color)
{
    m_freehandColor = color;
    // A swatch of the color, framed so that white shows on a white button.
    const QSize size = ui->freehandColorButton->iconSize();
    const qreal dpr = devicePixelRatioF();
    QPixmap swatch(size * dpr);
    swatch.setDevicePixelRatio(dpr);
    swatch.fill(color);
    QPainter painter(&swatch);
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawRect(QRectF(0, 0, size.width(), size.height()).adjusted(0.5, 0.5, -0.5, -0.5));
    painter.end();
    ui->freehandColorButton->setIcon(swatch);
    ui->freehandColorButton->setText(color.name(QColor::HexRgb).toUpper());
}

void SettingsDialog::showTranslateEngine()
{
    m_translateKeyEngine = ui->translateEngineCombo->currentData().toString();
    ui->translateKeyEdit->setText(m_translateKeys.value(m_translateKeyEngine));
    ui->translateEngineNote->setText(Translate::engineDescription(m_translateKeyEngine));
    // Only Azure asks for a region; the local model needs a model, not a key.
    const bool microsoft = m_translateKeyEngine == QLatin1String("microsoft");
    ui->translateRegionLabel->setVisible(microsoft);
    ui->translateRegionEdit->setVisible(microsoft);
    const bool local = m_translateKeyEngine == QLatin1String("local");
    const bool keyed = Translate::needsKey(m_translateKeyEngine);
    ui->translateKeyLabel->setVisible(keyed);
    ui->translateKeyEdit->setVisible(keyed);
    const bool firefox = m_translateKeyEngine == QLatin1String("firefox");
    ui->translateKeyNote->setVisible(!local && !firefox); // about sending text and storing keys
    ui->translateModelLabel->setVisible(local);
    ui->translateModelCombo->setVisible(local);
    const bool custom = local && ui->translateModelCombo->currentData().toString() == QLatin1String(LocalModel::kCustom);
    ui->translateModelFileLabel->setVisible(custom);
    ui->translateModelFileEdit->setVisible(custom);
    ui->translateModelFileButton->setVisible(custom);
    if (local || firefox) {
        const QString reason = local ? LocalModel::unavailableReason() : FirefoxTranslation::unavailableReason();
        if (!reason.isEmpty())
            ui->translateEngineNote->setText(ui->translateEngineNote->text() + QLatin1Char(' ') + reason);
    }
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
    if (!Updater::isSupported()) {
        ui->updateModeCombo->setEnabled(false);
        ui->updateModeCombo->setToolTip(Platform::unsupportedHint());
    }
    if (!Platform::supportsCursorCapture()) {
        ui->captureCursorCheck->setEnabled(false);
        ui->captureCursorCheck->setToolTip(Platform::unsupportedHint());
    }
    // Recording overlays need global input this platform does not offer.
    if (!Platform::supportsPointerHighlight()) {
        ui->recordHighlightCheck->setEnabled(false);
        ui->recordHighlightCheck->setToolTip(Platform::unsupportedHint());
    }
    if (!Platform::supportsInputOverlay()) {
        for (QCheckBox *check : {ui->recordClicksCheck, ui->recordKeysCheck}) {
            check->setEnabled(false);
            check->setToolTip(Platform::unsupportedHint());
        }
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
    ui->updateModeCombo->setCurrentIndex(int(Updater::Mode::Ask));
    ui->downloadOrderCombo->setCurrentIndex(int(ModelStore::SourceOrder::MirrorFirst));
    ui->downloadMirrorEdit->setText(ModelStore::defaultMirror());
    ui->captureToolbarCheck->setChecked(false);
    ui->captureCursorCheck->setChecked(false);
    ui->freehandTransparentCheck->setChecked(true);
    ui->recordHighlightCheck->setChecked(true);
    ui->recordClicksCheck->setChecked(true);
    ui->recordKeysCheck->setChecked(false);
    ui->recordKeyStyleCombo->setCurrentIndex(1);
    ui->recordFrameRateCombo->setCurrentIndex(std::max(0, ui->recordFrameRateCombo->findData(30)));
    ui->recordFolderEdit->setText(QDir::toNativeSeparators(AppSettings::defaultRecordFolder()));
    if (m_freehandColor != QColor(Qt::white)) {
        setFreehandColor(Qt::white);
        setModified(true);
    }
    ui->ocrEngineCombo->setCurrentIndex(std::max(0, ui->ocrEngineCombo->findData(Ocr::defaultEngine())));
    setOcrLanguages(Ocr::defaultLanguages());
    // Keys are the user's own and stay; engine and language go back.
    ui->translateEngineCombo->setCurrentIndex(std::max(0, ui->translateEngineCombo->findData(Translate::defaultEngine())));
    ui->translateTargetCombo->setCurrentIndex(std::max(0, ui->translateTargetCombo->findData(Translate::defaultTarget())));
    for (CaptureMode mode : kAllCaptureModes)
        hotkeyEdit(mode)->setKeySequence(AppSettings::defaultHotkey(mode));
    setToolbarItems(AppSettings::defaultToolbarItems());
    ui->toolbarIconSizeCombo->setCurrentIndex(std::max(0, ui->toolbarIconSizeCombo->findData(24)));
    setModified(true);
}

void SettingsDialog::setToolbarItems(const QList<AppSettings::ToolbarItem> &items)
{
    const QSignalBlocker blocker(ui->toolbarList);
    ui->toolbarList->clear();
    const QColor iconColor = palette().color(QPalette::ButtonText);
    for (const AppSettings::ToolbarItem &item : items) {
        QString icon = QStringLiteral(":/icons/timer.svg");
        for (CaptureMode mode : kAllCaptureModes) {
            if (AppSettings::toolbarItemId(mode) == item.id)
                icon = QString::fromLatin1(captureModeIcon(mode));
        }
        auto *row = new QListWidgetItem(tintedIcon(QIcon(icon), iconColor), QString(), ui->toolbarList);
        row->setData(Qt::UserRole, item.id);
        row->setFlags((row->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled) & ~Qt::ItemIsDropEnabled);
        row->setCheckState(item.visible ? Qt::Checked : Qt::Unchecked);
    }
    updateToolbarItemTexts();
    ui->toolbarList->setCurrentRow(0);
}

QList<AppSettings::ToolbarItem> SettingsDialog::toolbarItems() const
{
    QList<AppSettings::ToolbarItem> items;
    for (int i = 0; i < ui->toolbarList->count(); ++i) {
        const QListWidgetItem *row = ui->toolbarList->item(i);
        items.append({row->data(Qt::UserRole).toString(), row->checkState() == Qt::Checked});
    }
    return items;
}

void SettingsDialog::updateToolbarItemTexts()
{
    const QSignalBlocker blocker(ui->toolbarList);
    for (int i = 0; i < ui->toolbarList->count(); ++i) {
        QListWidgetItem *row = ui->toolbarList->item(i);
        const QString id = row->data(Qt::UserRole).toString();
        // The names the tray menu and the toolbar use, in their contexts.
        QString text = QCoreApplication::translate("CaptureToolbar", "Delay Before Capture");
        for (CaptureMode mode : kAllCaptureModes) {
            if (AppSettings::toolbarItemId(mode) == id)
                text = QCoreApplication::translate("CaptureController", captureModeLabel(mode));
        }
        row->setText(text);
    }
    const QStringList sizes = {tr("Small"), tr("Medium"), tr("Large")};
    for (int i = 0; i < ui->toolbarIconSizeCombo->count(); ++i) {
        const int size = ui->toolbarIconSizeCombo->itemData(i).toInt();
        ui->toolbarIconSizeCombo->setItemText(i, tr("%1 (%2 px)").arg(sizes.value(i), QString::number(size)));
    }
}

void SettingsDialog::moveToolbarItem(int delta)
{
    const int row = ui->toolbarList->currentRow();
    const int target = row + delta;
    if (row < 0 || target < 0 || target >= ui->toolbarList->count())
        return;
    QListWidgetItem *item = ui->toolbarList->takeItem(row);
    ui->toolbarList->insertItem(target, item);
    ui->toolbarList->setCurrentRow(target);
    setModified(true);
}
