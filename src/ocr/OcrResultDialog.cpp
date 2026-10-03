#include "OcrResultDialog.h"
#include "ui_OcrResultDialog.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QPainter>
#include <QtConcurrent/QtConcurrentRun>

namespace {

constexpr int kPreviewSize = 260; // logical px

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
    ui->textEdit->setReadOnly(true);
    ui->copyButton->setEnabled(false);

    connect(ui->copyButton, &QPushButton::clicked, this, &OcrResultDialog::copyText);
    connect(&m_watcher, &QFutureWatcher<OcrResult>::finished, this, &OcrResultDialog::onFinished);

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
    }
    QDialog::changeEvent(event);
}

void OcrResultDialog::onFinished()
{
    m_result = m_watcher.result();
    m_done = true;
    ui->textEdit->setReadOnly(false);
    ui->textEdit->setPlainText(m_result.text());
    ui->copyButton->setEnabled(!m_result.text().isEmpty());
    updatePreview();
    updateTexts();
}

void OcrResultDialog::updateTexts()
{
    const QString engine = Ocr::engineName(m_engine);
    if (!m_done)
        ui->statusLabel->setText(tr("Recognizing text with %1...").arg(engine));
    else if (!m_result.error.isEmpty())
        ui->statusLabel->setText(m_result.error);
    else if (m_result.text().trimmed().isEmpty())
        ui->statusLabel->setText(tr("No text was found in the selected area (%1).").arg(engine));
    else
        ui->statusLabel->setText(tr("Recognized with %1. You can edit the text before copying.").arg(engine));
}

void OcrResultDialog::updatePreview()
{
    QImage canvas = m_image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    canvas.setDevicePixelRatio(1.0);
    if (m_done) {
        QPainter p(&canvas);
        const qreal line = std::max(1.0, std::max(canvas.width(), canvas.height()) / 300.0);
        p.setPen(QPen(QColor(0x2d, 0x9c, 0xff), line));
        for (const OcrLine &l : std::as_const(m_result.lines)) {
            if (!l.box.isEmpty())
                p.drawRect(l.box);
        }
    }
    const qreal dpr = devicePixelRatioF();
    QPixmap pixmap = QPixmap::fromImage(
        canvas.scaled(QSize(kPreviewSize, kPreviewSize) * dpr, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    pixmap.setDevicePixelRatio(dpr);
    ui->preview->setPixmap(pixmap);
}

void OcrResultDialog::copyText()
{
    QGuiApplication::clipboard()->setText(ui->textEdit->toPlainText());
}
