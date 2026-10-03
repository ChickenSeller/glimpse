#include "ColorResultDialog.h"
#include "ui_ColorResultDialog.h"

#include "app/TintedIcon.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QPainter>

ColorResultDialog::ColorResultDialog(const QColor &color, QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::ColorResultDialog>())
    , m_color(color)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    // A swatch with a thin frame, so that white still shows on a white dialog.
    const QSize size = ui->swatch->minimumSize();
    const qreal dpr = devicePixelRatioF();
    QPixmap swatch(size * dpr);
    swatch.setDevicePixelRatio(dpr);
    swatch.fill(m_color);
    QPainter painter(&swatch);
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawRect(QRectF(0, 0, size.width(), size.height()).adjusted(0.5, 0.5, -0.5, -0.5));
    painter.end();
    ui->swatch->setPixmap(swatch);

    ui->hexEdit->setText(m_color.name(QColor::HexRgb).toUpper());
    ui->rgbEdit->setText(QStringLiteral("rgb(%1, %2, %3)").arg(m_color.red()).arg(m_color.green()).arg(m_color.blue()));
    const QColor hsl = m_color.toHsl();
    ui->hslEdit->setText(QStringLiteral("hsl(%1, %2%, %3%)")
                             .arg(std::max(0, hsl.hslHue())) // -1 for grays
                             .arg(qRound(hsl.hslSaturationF() * 100))
                             .arg(qRound(hsl.lightnessF() * 100)));

    connect(ui->hexCopyButton, &QToolButton::clicked, this, [this] { copy(ui->hexEdit); });
    connect(ui->rgbCopyButton, &QToolButton::clicked, this, [this] { copy(ui->rgbEdit); });
    connect(ui->hslCopyButton, &QToolButton::clicked, this, [this] { copy(ui->hslEdit); });
    connect(ui->pickAgainButton, &QPushButton::clicked, this, [this] {
        // Gone before the next grab, or it would be in the picture.
        hide();
        emit pickAgainRequested();
        close();
    });

    updateIcons();
    copy(ui->hexEdit);
}

ColorResultDialog::~ColorResultDialog() = default;

void ColorResultDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        updateTexts();
    } else if (event->type() == QEvent::PaletteChange) {
        updateIcons();
    }
    QDialog::changeEvent(event);
}

void ColorResultDialog::copy(const QLineEdit *field)
{
    m_copied = field->text();
    QGuiApplication::clipboard()->setText(m_copied);
    updateTexts();
}

void ColorResultDialog::updateTexts()
{
    ui->statusLabel->setText(tr("Copied %1 to the clipboard.").arg(m_copied));
}

void ColorResultDialog::updateIcons()
{
    const QIcon icon = tintedIcon(QIcon(QStringLiteral(":/icons/content_copy.svg")),
                                  palette().color(QPalette::ButtonText));
    for (QToolButton *button : {ui->hexCopyButton, ui->rgbCopyButton, ui->hslCopyButton})
        button->setIcon(icon);
}
