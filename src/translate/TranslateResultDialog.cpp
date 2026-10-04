#include "TranslateResultDialog.h"
#include "ui_TranslateResultDialog.h"

#include "Translator.h"
#include "app/AppSettings.h"

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
        // CJK text runs on without spaces; other scripts need one.
        const bool cjkJoin = isCjk(current->text.back()) && isCjk(text.front());
        current->text += (cjkJoin ? QString() : QStringLiteral(" ")) + text;
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

TranslateResultDialog::TranslateResultDialog(const QImage &image, const QString &ocrEngine,
                                             const QStringList &ocrLanguages, QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::TranslateResultDialog>())
    , m_image(image)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    const QStringList languages = Translate::targetLanguages();
    for (const QString &code : languages)
        ui->languageCombo->addItem(Translate::languageName(code), code);
    ui->languageCombo->setCurrentIndex(std::max(0, ui->languageCombo->findData(AppSettings::translateTarget())));

    connect(ui->translateButton, &QPushButton::clicked, this, &TranslateResultDialog::startTranslation);
    connect(ui->languageCombo, &QComboBox::currentIndexChanged, this, [this] {
        AppSettings::setTranslateTarget(ui->languageCombo->currentData().toString());
        if (m_state == State::Done || m_state == State::Failed)
            startTranslation();
    });
    connect(ui->copyButton, &QPushButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(ui->targetEdit->toPlainText());
    });
    connect(ui->imageButton, &QPushButton::clicked, this, [this] {
        emit translatedImageReady(paintTranslation(m_image, m_boxes, m_translations, m_lineHeights));
    });
    // Editing the source changes the paragraphs the boxes belong to.
    connect(ui->sourceEdit, &QPlainTextEdit::textChanged, this, &TranslateResultDialog::updateButtons);

    connect(&m_watcher, &QFutureWatcher<OcrResult>::finished, this, &TranslateResultDialog::onRecognized);
    m_watcher.setFuture(QtConcurrent::run([image, ocrEngine, ocrLanguages] {
        QString error;
        std::unique_ptr<OcrEngine> ocr = Ocr::createEngine(ocrEngine, ocrLanguages, &error);
        if (!ocr)
            return OcrResult{{}, error};
        return ocr->recognize(image);
    }));
    updateTexts();
    updateButtons();
}

TranslateResultDialog::~TranslateResultDialog()
{
    m_watcher.waitForFinished();
}

void TranslateResultDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        updateTexts();
    }
    QDialog::changeEvent(event);
}

void TranslateResultDialog::onRecognized()
{
    const OcrResult result = m_watcher.result();
    if (!result.error.isEmpty() || result.lines.isEmpty()) {
        m_state = State::Failed;
        m_error = result.error.isEmpty() ? tr("No text was found in the selected area.") : result.error;
        updateTexts();
        updateButtons();
        return;
    }
    const QList<Paragraph> paragraphs = paragraphsOf(result.lines);
    QStringList texts;
    for (const Paragraph &p : paragraphs) {
        texts << Ocr::tidyCjkSpacing(p.text);
        m_boxes << p.box;
        m_lineHeights << p.lineHeight;
    }
    {
        const QSignalBlocker blocker(ui->sourceEdit);
        ui->sourceEdit->setPlainText(texts.join(QStringLiteral("\n\n")));
    }
    startTranslation();
}

QStringList TranslateResultDialog::sourceParagraphs() const
{
    QStringList paragraphs;
    const QStringList parts =
        ui->sourceEdit->toPlainText().split(QRegularExpression(QStringLiteral("\\n\\s*\\n")), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString text = part.trimmed();
        if (!text.isEmpty())
            paragraphs << text;
    }
    return paragraphs;
}

void TranslateResultDialog::startTranslation()
{
    const QStringList paragraphs = sourceParagraphs();
    if (paragraphs.isEmpty())
        return;
    m_state = State::Translating;
    m_error.clear();
    const int request = ++m_request;
    const QString engine = AppSettings::translateEngine();
    Translate::translate(engine, paragraphs, ui->languageCombo->currentData().toString(), this,
                         [this, request](const QStringList &translations, const QString &error) {
                             if (request != m_request)
                                 return;
                             m_state = error.isEmpty() ? State::Done : State::Failed;
                             m_error = error;
                             m_translations = translations;
                             if (error.isEmpty())
                                 ui->targetEdit->setPlainText(translations.join(QStringLiteral("\n\n")));
                             updateTexts();
                             updateButtons();
                         });
    updateTexts();
    updateButtons();
}

void TranslateResultDialog::updateTexts()
{
    const QString engine = Translate::engineName(AppSettings::translateEngine());
    switch (m_state) {
    case State::Recognizing: ui->statusLabel->setText(tr("Recognizing text...")); break;
    case State::Translating: ui->statusLabel->setText(tr("Translating with %1...").arg(engine)); break;
    case State::Done: ui->statusLabel->setText(tr("Translated with %1.").arg(engine)); break;
    case State::Failed: ui->statusLabel->setText(m_error); break;
    }
}

void TranslateResultDialog::updateButtons()
{
    const bool idle = m_state == State::Done || m_state == State::Failed;
    ui->translateButton->setEnabled(idle && !sourceParagraphs().isEmpty());
    ui->copyButton->setEnabled(m_state == State::Done);
    // The image needs one translation per recognized box; after editing that
    // changed the number of paragraphs the boxes no longer match.
    const bool matches = m_translations.size() == m_boxes.size() && sourceParagraphs().size() == m_boxes.size();
    ui->imageButton->setEnabled(m_state == State::Done && matches);
    ui->imageButton->setToolTip(matches || m_state != State::Done
                                    ? tr("Paint the translation over the capture, where the text was")
                                    : tr("Keep the paragraphs as recognized (same number) to paint them over the capture."));
}
