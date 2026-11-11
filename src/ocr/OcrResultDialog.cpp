#include "OcrResultDialog.h"
#include "ui_OcrResultDialog.h"

#include "Paragraphs.h"
#include "Typesetter.h"
#include "app/AppSettings.h"
#include "translate/FirefoxTranslation.h"
#include "translate/LocalModel.h"
#include "translate/Translator.h"

#include <QAbstractItemView>
#include <QClipboard>
#include <QGuiApplication>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QPainter>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

OcrResultDialog::OcrResultDialog(const QImage &image, const QString &engine, const QStringList &languages,
                                 QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::OcrResultDialog>())
    , m_image(image)
    , m_engine(engine)
    , m_languages(languages)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    // Room for long texts: the window can be maximized.
    setWindowFlag(Qt::WindowMaximizeButtonHint);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    ui->sourceEdit->setReadOnly(true);
    ui->engineCombo->installEventFilter(this);
    ui->ocrEngineCombo->installEventFilter(this);
    // The capture gets what the text needs less of.
    ui->splitter->setSizes({200, 260, 260});
    ui->preview->installEventFilter(this);

    const QStringList targets = Translate::targetLanguages();
    for (const QString &code : targets)
        ui->languageCombo->addItem(Translate::languageName(code), code);
    ui->languageCombo->setCurrentIndex(std::max(0, ui->languageCombo->findData(AppSettings::translateTarget())));
    // Engine names are long ("Hy-MT2 7B (4.6 GB, ...)"): the list shows them whole.
    ui->engineCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    ui->engineCombo->setMinimumContentsLength(24);
    fillEngines();
    fillOcrEngines();

    connect(ui->copyButton, &QPushButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(ui->sourceEdit->toPlainText());
    });
    connect(ui->translateButton, &QPushButton::clicked, this, &OcrResultDialog::translateRequested);
    connect(ui->languageCombo, &QComboBox::currentIndexChanged, this, [this] {
        AppSettings::setTranslateTarget(ui->languageCombo->currentData().toString());
        // Once translated, a new language translates again.
        if (m_translation == Translation::Done || m_translation == Translation::Failed)
            emit translateRequested();
    });
    // For this window only: the settings keep the engine it opens with.
    connect(ui->engineCombo, &QComboBox::currentIndexChanged, this, [this] {
        updateComboTips();
        updatePrompt();
        if (m_translation != Translation::None)
            emit translateRequested();
        else
            updateTexts();
    });
    // For this window only too; the owner readies the engine, then recognize().
    connect(ui->ocrEngineCombo, &QComboBox::currentIndexChanged, this, [this] {
        const QString chosen = ui->ocrEngineCombo->currentData().toString();
        if (!chosen.isEmpty() && chosen != m_engine)
            emit recognizeRequested(chosen);
    });
    connect(ui->copyTranslationButton, &QPushButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(ui->targetEdit->toPlainText());
    });
    connect(ui->imageButton, &QPushButton::clicked, this, [this] {
        const QStringList texts = paintedTranslations();
        QList<Typesetter::Paragraph> paragraphs;
        for (qsizetype i = 0; i < m_boxes.size() && i < texts.size(); ++i)
            paragraphs.append({m_boxes.at(i), m_lineBoxes.value(i), texts.at(i), m_paragraphs.value(i)});
        emit translatedImageReady(Typesetter::typeset(m_image, paragraphs));
    });
    // Editing the text changes the paragraphs the boxes belong to.
    connect(ui->sourceEdit, &QPlainTextEdit::textChanged, this, &OcrResultDialog::updateButtons);
    connect(ui->targetEdit, &QPlainTextEdit::textChanged, this, &OcrResultDialog::updateButtons);
    connect(&m_watcher, &QFutureWatcher<OcrResult>::finished, this, &OcrResultDialog::onRecognized);

    // The prompt of a language model, for this window only: it starts as the
    // settings' one, kept per engine as edited here.
    ui->promptEdit->setVisible(false);
    ui->promptResetButton->setVisible(false);
    connect(ui->promptButton, &QToolButton::toggled, this, [this](bool on) {
        updatePrompt();
        if (on)
            ui->promptEdit->setFocus();
    });
    connect(ui->promptResetButton, &QPushButton::clicked, this, [this] {
        m_prompts.remove(translator());
        updatePrompt();
    });
    connect(ui->promptEdit, &QPlainTextEdit::textChanged, this, [this] {
        m_prompts.insert(translator(), ui->promptEdit->toPlainText());
        updatePrompt();
    });
    updatePrompt();

    recognize(engine);
}

void OcrResultDialog::recognize(const QString &engine)
{
    m_engine = engine;
    showOcrEngine(engine);
    // What came from the last engine goes; a translation of it is made again.
    m_recognized = false;
    m_result = {};
    m_recognizedText.clear();
    m_paragraphs.clear();
    m_boxes.clear();
    m_lineHeights.clear();
    m_lineBoxes.clear();
    m_translations.clear();
    m_translateAgain = m_translation != Translation::None;
    m_translation = Translation::None;
    m_translateError.clear();
    ++m_request; // a translation still running is of the old text
    {
        const QSignalBlocker blocker(ui->sourceEdit);
        ui->sourceEdit->clear();
    }
    ui->sourceEdit->setReadOnly(true);
    ui->targetEdit->clear();
    ui->targetEdit->setReadOnly(true);
    m_translatedText.clear();

    // Engines load models and recognize on a worker thread; the engine object
    // never leaves it. A recognition still running is left to finish unseen.
    const QImage image = m_image;
    const QStringList languages = m_languages;
    m_watcher.setFuture(QtConcurrent::run([image, engine, languages] {
        QString error;
        std::unique_ptr<OcrEngine> ocr = Ocr::createEngine(engine, languages, &error);
        if (!ocr)
            return OcrResult{{}, error};
        return ocr->recognize(image);
    }));

    updatePreview();
    updateTexts();
    updateButtons();
}

void OcrResultDialog::showOcrEngine(const QString &engine)
{
    {
        const QSignalBlocker blocker(ui->ocrEngineCombo);
        ui->ocrEngineCombo->setCurrentIndex(std::max(0, ui->ocrEngineCombo->findData(engine)));
    }
    updateComboTips();
}

// The engines this build can run here, and the one in use.
void OcrResultDialog::fillOcrEngines()
{
    const QSignalBlocker blocker(ui->ocrEngineCombo);
    ui->ocrEngineCombo->clear();
    const QStringList engines = Ocr::engineIds();
    for (const QString &engine : engines) {
        if (engine == m_engine || Ocr::engineUnavailableReason(engine).isEmpty())
            ui->ocrEngineCombo->addItem(Ocr::engineName(engine), engine);
    }
    showOcrEngine(m_engine);
}

OcrResultDialog::~OcrResultDialog()
{
    // Closing mid-recognition: let the worker finish rather than leave it
    // writing into a destroyed watcher.
    m_watcher.waitForFinished();
}

void OcrResultDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        fillEngines();
        fillOcrEngines();
        updatePrompt();
        updateComboTips();
        updateTexts();
        updateButtons();
    }
    QDialog::changeEvent(event);
}

bool OcrResultDialog::eventFilter(QObject *watched, QEvent *event)
{
    // Window or splitter: the capture is scaled to the room it has.
    if (watched == ui->preview && event->type() == QEvent::Resize)
        updatePreview();
    // A narrower box may cut the engine's name.
    if ((watched == ui->engineCombo || watched == ui->ocrEngineCombo) && event->type() == QEvent::Resize)
        updateComboTips();
    return QDialog::eventFilter(watched, event);
}

namespace {

// Whether the combo box is too narrow to show its current choice whole.
bool cutShort(const QComboBox *combo)
{
    QStyleOptionComboBox option;
    option.initFrom(combo);
    option.currentText = combo->currentText();
    const QRect field = combo->style()->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, combo);
    return combo->fontMetrics().horizontalAdvance(combo->currentText()) > field.width();
}

void setComboTip(QComboBox *combo, const QString &purpose)
{
    combo->setToolTip(cutShort(combo) ? combo->currentText() + QStringLiteral("\n\n") + purpose : purpose);
}

} // namespace

void OcrResultDialog::updateComboTips()
{
    // The list itself always shows the names whole, however narrow the box.
    for (QComboBox *combo : {ui->engineCombo, ui->ocrEngineCombo})
        combo->view()->setMinimumWidth(combo->view()->sizeHintForColumn(0) + 2 * combo->view()->frameWidth());
    // The choice in full when cut short; what the box is for either way.
    setComboTip(ui->engineCombo, tr("The translation engine or model for this window; Settings > Translation sets "
                                    "the one it opens with"));
    setComboTip(ui->ocrEngineCombo, tr("The text recognition engine for this window; Settings > Text Recognition "
                                       "sets the one it opens with"));
}

void OcrResultDialog::onRecognized()
{
    m_result = m_watcher.result();
    m_recognized = true;
    const QList<Ocr::Paragraph> paragraphs = Ocr::paragraphsOf(m_result.lines);
    for (const Ocr::Paragraph &p : paragraphs) {
        m_paragraphs << Ocr::tidyCjkSpacing(p.text);
        m_boxes << p.box;
        m_lineHeights << p.lineHeight;
        m_lineBoxes << p.lines;
    }
    // Shown as paragraphs, one per line and laid out as on the capture, so
    // that the lines edited are still the paragraphs to translate.
    m_recognizedText = layOut(m_paragraphs);
    {
        const QSignalBlocker blocker(ui->sourceEdit);
        ui->sourceEdit->setPlainText(m_recognizedText);
    }
    ui->sourceEdit->setReadOnly(false);
    // The splitter moved the preview's size around as the text came in.
    updatePreview();
    updateTexts();
    updateButtons();
    if (m_translateAgain) {
        m_translateAgain = false;
        if (!m_paragraphs.isEmpty())
            emit translateRequested();
    }
}

bool OcrResultDialog::translationEdited() const
{
    return m_translation == Translation::Done && ui->targetEdit->toPlainText() != m_translatedText;
}

bool OcrResultDialog::sourceEdited() const
{
    return ui->sourceEdit->toPlainText() != m_recognizedText;
}

QStringList OcrResultDialog::sourceParagraphs() const
{
    if (!sourceEdited())
        return m_paragraphs;
    // Each line a paragraph, as shown; blank lines only space them.
    QStringList paragraphs;
    for (const QString &line : ui->sourceEdit->toPlainText().split(QLatin1Char('\n'))) {
        if (!line.trimmed().isEmpty())
            paragraphs << line.trimmed();
    }
    return paragraphs;
}

// Every engine that can translate now (or is the settings' choice, which
// then says what it lacks), the local engine once per model.
void OcrResultDialog::fillEngines()
{
    QStringList current = ui->engineCombo->currentData().toStringList();
    const QString defaultEngine = AppSettings::translateEngine();
    const QString defaultModel = AppSettings::localModel();
    if (current.isEmpty())
        current = {defaultEngine, defaultEngine == QLatin1String("local") ? defaultModel : QString()};

    const QSignalBlocker blocker(ui->engineCombo);
    ui->engineCombo->clear();
    const QStringList engines = Translate::engineIds();
    for (const QString &engine : engines) {
        const bool chosen = engine == defaultEngine;
        if (engine == QLatin1String("local")) {
            if (!chosen && !LocalModel::unavailableReason().isEmpty())
                continue;
            QStringList models;
            const QList<LocalModel::Preset> presets = LocalModel::presets();
            for (const LocalModel::Preset &preset : presets)
                models << preset.id;
            if (!AppSettings::localModelFile().trimmed().isEmpty() || defaultModel == QLatin1String(LocalModel::kCustom))
                models << QString::fromLatin1(LocalModel::kCustom);
            for (const QString &model : std::as_const(models))
                ui->engineCombo->addItem(tr("Local: %1").arg(LocalModel::modelName(model)), QStringList{engine, model});
            continue;
        }
        if (!chosen && engine == QLatin1String("firefox") && !FirefoxTranslation::unavailableReason().isEmpty())
            continue;
        if (!chosen && !Translate::isConfigured(engine))
            continue;
        ui->engineCombo->addItem(Translate::engineName(engine), QStringList{engine, QString()});
    }
    ui->engineCombo->setCurrentIndex(std::max(0, ui->engineCombo->findData(current)));
    updateComboTips();
}

QString OcrResultDialog::translator() const
{
    const QStringList choice = ui->engineCombo->currentData().toStringList();
    return choice.isEmpty() ? AppSettings::translateEngine() : choice.constFirst();
}

QString OcrResultDialog::localModel() const
{
    const QStringList choice = ui->engineCombo->currentData().toStringList();
    return choice.size() > 1 ? choice.at(1) : QString();
}

bool OcrResultDialog::takesPrompt() const
{
    // A local model made for translation keeps its documented prompt.
    return Translate::takesPrompt(translator())
           && !(translator() == QLatin1String("local") && LocalModel::hasOwnPrompt(LocalModel::modelPath(localModel())));
}

void OcrResultDialog::updatePrompt()
{
    const bool takes = takesPrompt();
    ui->promptButton->setVisible(takes);
    ui->promptEdit->setVisible(takes && ui->promptButton->isChecked());
    ui->promptResetButton->setVisible(ui->promptEdit->isVisibleTo(this));
    if (!takes)
        return;
    const QString engine = translator();
    const QString text = m_prompts.contains(engine) ? m_prompts.value(engine) : Translate::settingsPrompt(engine);
    if (ui->promptEdit->toPlainText() != text) {
        const QSignalBlocker blocker(ui->promptEdit);
        ui->promptEdit->setPlainText(text);
    }
    // Bold while this window's prompt differs from the settings'.
    QFont font = ui->promptButton->font();
    font.setBold(text.trimmed() != Translate::settingsPrompt(engine));
    ui->promptButton->setFont(font);
    ui->promptResetButton->setEnabled(font.bold());
    ui->promptButton->setToolTip(font.bold() ? tr("The prompt was changed for this window")
                                             : tr("Show the prompt, to change it for this window"));
}

QString OcrResultDialog::translatorName() const
{
    return translator() == QLatin1String("local") ? LocalModel::modelName(localModel())
                                                  : Translate::engineName(translator());
}

void OcrResultDialog::translate()
{
    const QStringList paragraphs = sourceParagraphs();
    if (paragraphs.isEmpty())
        return;
    m_translation = Translation::Running;
    m_translateError.clear();
    const int request = ++m_request;
    // The text edited: the translation takes its lines and blank lines.
    const QString layout = sourceEdited() ? ui->sourceEdit->toPlainText() : QString();
    Translate::translate(translator(), paragraphs, ui->languageCombo->currentData().toString(), this,
                         [this, request, layout](const QStringList &translations, const QString &error) {
                             if (request != m_request)
                                 return;
                             m_translation = error.isEmpty() ? Translation::Done : Translation::Failed;
                             m_translateError = error;
                             // Each one line, so that a line edited is a paragraph.
                             m_translations.clear();
                             for (const QString &t : translations)
                                 m_translations << t.simplified();
                             if (error.isEmpty()) {
                                 m_translatedText = layout.isEmpty() ? layOut(m_translations)
                                                                     : followLayout(layout, m_translations);
                                 ui->targetEdit->setPlainText(m_translatedText);
                                 ui->targetEdit->setReadOnly(false);
                             }
                             updateTexts();
                             updateButtons();
                         },
                         localModel(), takesPrompt() ? ui->promptEdit->toPlainText() : QString());
    updateTexts();
    updateButtons();
}

QStringList OcrResultDialog::paintedTranslations() const
{
    const QString text = ui->targetEdit->toPlainText();
    if (text == m_translatedText)
        return m_translations;
    // Line by line in the order of the paragraphs; blank lines only space
    // them. Lines short leave the last paragraphs empty (the text erased),
    // lines over join the last one.
    QStringList lines;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        if (!line.trimmed().isEmpty())
            lines << line.trimmed();
    }
    QStringList texts;
    for (qsizetype i = 0; i < m_boxes.size(); ++i)
        texts << lines.value(i);
    if (!texts.isEmpty() && lines.size() > texts.size())
        texts.last() = lines.mid(texts.size() - 1).join(QLatin1Char(' '));
    return texts;
}

// One paragraph after another as they were laid out: a paragraph right below
// the one before it (the next line of a listing) on the next line, others
// after a blank line.
QString OcrResultDialog::layOut(const QStringList &texts) const
{
    if (texts.size() != m_boxes.size())
        return texts.join(QStringLiteral("\n\n"));
    QString text;
    for (qsizetype i = 0; i < texts.size(); ++i) {
        if (i > 0) {
            const QRect &above = m_boxes.at(i - 1);
            const QRect &below = m_boxes.at(i);
            const bool overlaps = below.left() < above.right() && below.right() > above.left();
            const bool next = overlaps && below.top() - above.bottom() < m_lineHeights.at(i - 1) * 0.8;
            text += next ? QStringLiteral("\n") : QStringLiteral("\n\n");
        }
        text += texts.at(i);
    }
    return text;
}

// The translations in place of the edited text's lines, its blank lines kept.
QString OcrResultDialog::followLayout(const QString &source, const QStringList &translations) const
{
    QStringList lines;
    qsizetype next = 0;
    for (const QString &line : source.split(QLatin1Char('\n')))
        lines << (line.trimmed().isEmpty() ? QString() : translations.value(next++));
    while (next < translations.size()) // more than asked for: keep them
        lines << translations.at(next++);
    return lines.join(QLatin1Char('\n')).trimmed();
}

void OcrResultDialog::updateTexts()
{
    const QString ocrEngine = Ocr::engineName(m_engine);
    const QString translator = translatorName();
    QString status;
    if (!m_recognized)
        status = tr("Recognizing text with %1...").arg(ocrEngine);
    else if (!m_result.error.isEmpty())
        status = m_result.error;
    else if (m_result.text().trimmed().isEmpty())
        status = tr("No text was found in the selected area (%1).").arg(ocrEngine);
    else if (m_translation == Translation::Running)
        status = tr("Translating with %1...").arg(translator);
    else if (m_translation == Translation::Failed)
        status = m_translateError;
    else if (m_translation == Translation::Done)
        status = tr("Translated with %1.").arg(translator);
    else
        status = tr("Recognized with %1. You can edit the text before copying or translating.").arg(ocrEngine);
    ui->statusLabel->setText(status);
}

void OcrResultDialog::updateButtons()
{
    const bool hasText = !ui->sourceEdit->toPlainText().trimmed().isEmpty();
    // Either text changed by hand says so, until it is recognized or
    // translated anew.
    ui->sourceEditedLabel->setVisible(m_recognized && sourceEdited());
    ui->targetEditedLabel->setVisible(translationEdited());
    ui->copyButton->setEnabled(hasText);
    ui->translateButton->setEnabled(m_recognized && hasText && m_translation != Translation::Running);
    ui->copyTranslationButton->setEnabled(m_translation == Translation::Done);
    // The image needs one translation per recognized box: as many lines in
    // the text translated as paragraphs were recognized.
    const bool matches = !m_boxes.isEmpty() && m_translations.size() == m_boxes.size();
    ui->imageButton->setEnabled(m_translation == Translation::Done && matches);
    ui->imageButton->setToolTip(matches || m_translation != Translation::Done
                                    ? tr("Paint the translation over the capture, where the text was")
                                    : tr("Keep one line per recognized paragraph in the text, then translate it again "
                                         "to paint the translation over the capture."));
}

void OcrResultDialog::updatePreview()
{
    const QSize room = ui->preview->contentsRect().size();
    if (room.isEmpty())
        return;
    QImage canvas = m_image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    canvas.setDevicePixelRatio(1.0);
    if (m_recognized) {
        QPainter p(&canvas);
        const qreal line = std::max(1.0, std::max(canvas.width(), canvas.height()) / 300.0);
        p.setPen(QPen(QColor(0x2d, 0x9c, 0xff), line));
        for (const OcrLine &l : std::as_const(m_result.lines)) {
            if (!l.box.isEmpty())
                p.drawRect(l.box);
        }
    }
    // Never larger than the capture itself, sharp on scaled screens.
    const qreal dpr = devicePixelRatioF();
    const QSize target = (QSizeF(room) * dpr).toSize().boundedTo(canvas.size());
    QPixmap pixmap = QPixmap::fromImage(canvas.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    pixmap.setDevicePixelRatio(dpr);
    ui->preview->setPixmap(pixmap);
}
