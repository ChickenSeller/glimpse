#include "QrResultDialog.h"
#include "ui_QrResultDialog.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QPainter>
#include <QUrl>

namespace {

constexpr int kPreviewSize = 260; // logical px

const QColor kOutline(0x2d, 0x9c, 0xff);

// Only schemes that are safe to hand to the default handler; a QR code is
// untrusted input and could carry file:, javascript: or custom app schemes.
bool isOpenableUrl(const QString &text)
{
    const QUrl url(text.trimmed(), QUrl::StrictMode);
    static const QStringList schemes = {QStringLiteral("http"), QStringLiteral("https"),
                                        QStringLiteral("mailto"), QStringLiteral("tel")};
    return url.isValid() && schemes.contains(url.scheme().toLower());
}

QPixmap renderPreview(const QImage &image, const QList<ScannedCode> &codes, qreal dpr)
{
    QImage canvas = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    canvas.setDevicePixelRatio(1.0);
    {
        QPainter p(&canvas);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal line = std::max(2.0, std::max(canvas.width(), canvas.height()) / 200.0);
        QFont font = p.font();
        font.setPixelSize(int(line * 8));
        font.setBold(true);
        p.setFont(font);
        for (int i = 0; i < codes.size(); ++i) {
            p.setPen(QPen(kOutline, line));
            p.setBrush(QColor(0x2d, 0x9c, 0xff, 40));
            p.drawPolygon(codes[i].corners);
            // Number badge at the code's first corner, matching the list.
            const QRectF badge(codes[i].corners.boundingRect().topLeft(), QSizeF(line * 11, line * 11));
            p.setPen(Qt::NoPen);
            p.setBrush(kOutline);
            p.drawEllipse(badge);
            p.setPen(Qt::white);
            p.drawText(badge, Qt::AlignCenter, QString::number(i + 1));
        }
    }

    QPixmap pixmap = QPixmap::fromImage(canvas.scaled(QSize(kPreviewSize, kPreviewSize) * dpr, Qt::KeepAspectRatio,
                                                      Qt::SmoothTransformation));
    pixmap.setDevicePixelRatio(dpr);
    return pixmap;
}

} // namespace

QrResultDialog::QrResultDialog(const QImage &image, const QList<ScannedCode> &codes, QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::QrResultDialog>())
    , m_codes(codes)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->preview->setPixmap(renderPreview(image, m_codes, devicePixelRatioF()));

    for (int i = 0; i < m_codes.size(); ++i) {
        const QString firstLine = m_codes[i].text.section(QLatin1Char('\n'), 0, 0);
        ui->resultList->addItem(QStringLiteral("%1. [%2] %3").arg(i + 1).arg(m_codes[i].format, firstLine));
    }

    const bool found = !m_codes.isEmpty();
    ui->resultList->setVisible(m_codes.size() > 1);
    ui->textView->setVisible(found);
    ui->copyButton->setEnabled(found);

    connect(ui->resultList, &QListWidget::currentRowChanged, this, &QrResultDialog::showCode);
    connect(ui->copyButton, &QPushButton::clicked, this, &QrResultDialog::copySelected);
    connect(ui->openButton, &QPushButton::clicked, this, &QrResultDialog::openSelected);

    updateTexts();
    if (found)
        ui->resultList->setCurrentRow(0);
    else
        showCode(-1);
}

QrResultDialog::~QrResultDialog() = default;

void QrResultDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        updateTexts();
    }
    QDialog::changeEvent(event);
}

void QrResultDialog::updateTexts()
{
    if (m_codes.isEmpty())
        ui->summaryLabel->setText(tr("No QR code or barcode was found in the selected area."));
    else
        ui->summaryLabel->setText(tr("Found %n code(s).", nullptr, int(m_codes.size())));
}

void QrResultDialog::showCode(int row)
{
    const bool valid = row >= 0 && row < m_codes.size();
    ui->textView->setPlainText(valid ? m_codes[row].text : QString());
    ui->openButton->setEnabled(valid && isOpenableUrl(m_codes[row].text));
}

QString QrResultDialog::selectedText() const
{
    const int row = ui->resultList->currentRow();
    return row >= 0 && row < m_codes.size() ? m_codes[row].text : QString();
}

void QrResultDialog::copySelected()
{
    QGuiApplication::clipboard()->setText(selectedText());
}

void QrResultDialog::openSelected()
{
    const QString text = selectedText();
    if (isOpenableUrl(text))
        QDesktopServices::openUrl(QUrl(text.trimmed(), QUrl::StrictMode));
}
