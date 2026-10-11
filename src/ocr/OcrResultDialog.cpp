#include "OcrResultDialog.h"
#include "ui_OcrResultDialog.h"

#include "app/AppSettings.h"
#include "translate/Translator.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QPainter>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace {

struct Paragraph {
    QString text;
    QRect box;
    int lineHeight = 0;
};

bool isCjk(QChar c)
{
    const char16_t u = c.unicode();
    return (u >= 0x3040 && u <= 0x30ff) || (u >= 0x3400 && u <= 0x9fff) || (u >= 0xac00 && u <= 0xd7af)
           || (u >= 0xff00 && u <= 0xffef);
}

// Appends `text` to `paragraph`: CJK text runs on without spaces; other
// scripts need one.
void joinInto(QString &paragraph, const QString &text)
{
    if (paragraph.isEmpty() || text.isEmpty()) {
        paragraph += text;
        return;
    }
    const bool cjkJoin = isCjk(paragraph.back()) && isCjk(text.front());
    paragraph += (cjkJoin ? QString() : QStringLiteral(" ")) + text;
}

// Joins OCR lines into paragraphs: a line continues the paragraph above when
// it starts close below it and overlaps it horizontally. Translating whole
// paragraphs gives far better results than line by line.
QList<Paragraph> paragraphsOf(QList<OcrLine> lines)
{
    std::sort(lines.begin(), lines.end(), [](const OcrLine &a, const OcrLine &b) { return a.box.top() < b.box.top(); });
    QList<Paragraph> paragraphs;
    for (const OcrLine &line : std::as_const(lines)) {
        const QString text = line.text.trimmed();
        if (text.isEmpty())
            continue;
        Paragraph *current = nullptr;
        for (Paragraph &p : paragraphs) {
            const int gap = line.box.top() - p.box.bottom();
            const bool overlaps = line.box.left() < p.box.right() && line.box.right() > p.box.left();
            if (overlaps && gap > -p.lineHeight / 2 && gap < p.lineHeight * 0.8)
                current = &p;
        }
        if (!current) {
            paragraphs.append({text, line.box, line.box.height()});
            continue;
        }
        joinInto(current->text, text);
        current->box |= line.box;
    }
    // Reading order: top to bottom, then left to right.
    std::sort(paragraphs.begin(), paragraphs.end(), [](const Paragraph &a, const Paragraph &b) {
        return a.box.top() == b.box.top() ? a.box.left() < b.box.left() : a.box.top() < b.box.top();
    });
    return paragraphs;
}

// The average color of the pixels just around `box`: the background the
// text sat on, used to cover the original text.
QColor surroundingColor(const QImage &image, const QRect &box)
{
    const QRect ring = box.adjusted(-3, -3, 3, 3).intersected(image.rect());
    qint64 r = 0, g = 0, b = 0, n = 0;
    const auto add = [&](int x, int y) {
        const QRgb px = image.pixel(x, y);
        r += qRed(px);
        g += qGreen(px);
        b += qBlue(px);
        ++n;
    };
    for (int x = ring.left(); x <= ring.right(); ++x) {
        add(x, ring.top());
        add(x, ring.bottom());
    }
    for (int y = ring.top(); y <= ring.bottom(); ++y) {
        add(ring.left(), y);
        add(ring.right(), y);
    }
    return n ? QColor(int(r / n), int(g / n), int(b / n)) : QColor(Qt::white);
}

// The capture with each paragraph's text replaced by its translation, in the
// largest font (up to the original size) that fits the paragraph's box.
QImage paintTranslation(const QImage &source, const QList<QRect> &boxes, const QStringList &texts,
                        const QList<int> &lineHeights)
{
    QImage image = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const qreal dpr = image.devicePixelRatio();
    image.setDevicePixelRatio(1.0); // boxes are in image pixels
    QPainter p(&image);
    p.setRenderHint(QPainter::TextAntialiasing);
    for (qsizetype i = 0; i < boxes.size() && i < texts.size(); ++i) {
        const QRect box = boxes[i].adjusted(-2, -2, 2, 2).intersected(image.rect());
        const QColor background = surroundingColor(source, boxes[i]);
        p.fillRect(box, background);
        p.setPen(qGray(background.rgb()) > 140 ? QColor(0x20, 0x20, 0x20) : QColor(0xf5, 0xf5, 0xf5));
        QFont font = p.font();
        int flags = Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap;
        for (int size = std::max(8, int(lineHeights.value(i, 16) * 0.8)); size >= 6; --size) {
            font.setPixelSize(size);
            const QRect needed = QFontMetrics(font).boundingRect(box, flags, texts[i]);
            if (needed.height() <= box.height() && needed.width() <= box.width())
                break;
            if (size == 6)
                flags = Qt::AlignLeft | Qt::AlignTop | Qt::TextWrapAnywhere;
        }
        p.setFont(font);
        p.drawText(box, flags, texts[i]);
    }
    p.end();
    image.setDevicePixelRatio(dpr);
    return image;
}

} // namespace

OcrResultDialog::OcrResultDialog(const QImage &image, const QString &engine, const QStringList &languages,
                                 QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::OcrResultDialog>())
    , m_image(image)
    , m_engine(engine)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    ui->sourceEdit->setReadOnly(true);
    // The capture gets what the text needs less of.
    ui->splitter->setSizes({200, 260, 260});
    ui->preview->installEventFilter(this);

    const QStringList targets = Translate::targetLanguages();
    for (const QString &code : targets)
        ui->languageCombo->addItem(Translate::languageName(code), code);
    ui->languageCombo->setCurrentIndex(std::max(0, ui->languageCombo->findData(AppSettings::translateTarget())));

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
    connect(ui->copyTranslationButton, &QPushButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(ui->targetEdit->toPlainText());
    });
    connect(ui->imageButton, &QPushButton::clicked, this, [this] {
        emit translatedImageReady(paintTranslation(m_image, m_boxes, m_translations, m_lineHeights));
    });
    // Editing the text changes the paragraphs the boxes belong to.
    connect(ui->sourceEdit, &QPlainTextEdit::textChanged, this, &OcrResultDialog::updateButtons);
    connect(&m_watcher, &QFutureWatcher<OcrResult>::finished, this, &OcrResultDialog::onRecognized);

    // Engines load models and recognize on a worker thread; the engine object
    // never leaves it.
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
    return QDialog::eventFilter(watched, event);
}

void OcrResultDialog::onRecognized()
{
    m_result = m_watcher.result();
    m_recognized = true;
    const QList<Paragraph> paragraphs = paragraphsOf(m_result.lines);
    for (const Paragraph &p : paragraphs) {
        m_paragraphs << Ocr::tidyCjkSpacing(p.text);
        m_boxes << p.box;
        m_lineHeights << p.lineHeight;
    }
    m_recognizedText = m_result.text();
    {
        const QSignalBlocker blocker(ui->sourceEdit);
        ui->sourceEdit->setPlainText(m_recognizedText);
    }
    ui->sourceEdit->setReadOnly(false);
    // The splitter moved the preview's size around as the text came in.
    updatePreview();
    updateTexts();
    updateButtons();
}

bool OcrResultDialog::sourceEdited() const
{
    return ui->sourceEdit->toPlainText() != m_recognizedText;
}

QStringList OcrResultDialog::sourceParagraphs() const
{
    if (!sourceEdited())
        return m_paragraphs;
    QStringList paragraphs;
    const QStringList parts =
        ui->sourceEdit->toPlainText().split(QRegularExpression(QStringLiteral("\\n\\s*\\n")), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        QString paragraph;
        for (const QString &line : part.split(QLatin1Char('\n')))
            joinInto(paragraph, line.trimmed());
        if (!paragraph.isEmpty())
            paragraphs << paragraph;
    }
    return paragraphs;
}

void OcrResultDialog::translate()
{
    const QStringList paragraphs = sourceParagraphs();
    if (paragraphs.isEmpty())
        return;
    m_translation = Translation::Running;
    m_translateError.clear();
    const int request = ++m_request;
    const bool edited = sourceEdited();
    Translate::translate(AppSettings::translateEngine(), paragraphs, ui->languageCombo->currentData().toString(), this,
                         [this, request, edited](const QStringList &translations, const QString &error) {
                             if (request != m_request)
                                 return;
                             m_translation = error.isEmpty() ? Translation::Done : Translation::Failed;
                             m_translateError = error;
                             // Only the recognized paragraphs have boxes to paint into.
                             m_translations = edited ? QStringList() : translations;
                             if (error.isEmpty())
                                 ui->targetEdit->setPlainText(translations.join(QStringLiteral("\n\n")));
                             updateTexts();
                             updateButtons();
                         });
    updateTexts();
    updateButtons();
}

void OcrResultDialog::updateTexts()
{
    const QString ocrEngine = Ocr::engineName(m_engine);
    const QString translator = Translate::engineName(AppSettings::translateEngine());
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
    ui->copyButton->setEnabled(hasText);
    ui->translateButton->setEnabled(m_recognized && hasText && m_translation != Translation::Running);
    ui->copyTranslationButton->setEnabled(m_translation == Translation::Done);
    // The image needs one translation per recognized box: the text as
    // recognized, translated as such.
    const bool matches = !sourceEdited() && !m_boxes.isEmpty() && m_translations.size() == m_boxes.size();
    ui->imageButton->setEnabled(m_translation == Translation::Done && matches);
    ui->imageButton->setToolTip(matches || m_translation != Translation::Done
                                    ? tr("Paint the translation over the capture, where the text was")
                                    : tr("Translate the text as recognized, unedited, to paint it over the capture."));
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
