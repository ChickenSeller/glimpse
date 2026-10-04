#include "RecordingDoneDialog.h"
#include "ui_RecordingDoneDialog.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QUrl>

RecordingDoneDialog::RecordingDoneDialog(const QString &filePath, qint64 durationMs, QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::RecordingDoneDialog>())
    , m_path(QDir::toNativeSeparators(filePath))
    , m_durationMs(durationMs)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    ui->pathEdit->setText(m_path);

    connect(ui->openButton, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_path));
    });
    connect(ui->folderButton, &QPushButton::clicked, this, [this] {
#ifdef Q_OS_WIN
        // Opens the folder with the file selected.
        QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), m_path});
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_path).absolutePath()));
#endif
    });
    connect(ui->copyPathButton, &QPushButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(m_path);
        ui->copyPathButton->setText(tr("Copied"));
    });
    updateTexts();
}

RecordingDoneDialog::~RecordingDoneDialog() = default;

void RecordingDoneDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        updateTexts();
    }
    QDialog::changeEvent(event);
}

void RecordingDoneDialog::updateTexts()
{
    const qint64 s = m_durationMs / 1000;
    const double mb = QFileInfo(m_path).size() / (1024.0 * 1024.0);
    ui->summaryLabel->setText(tr("Recorded %1:%2 (%3 MB):")
                                  .arg(s / 60, 2, 10, QLatin1Char('0'))
                                  .arg(s % 60, 2, 10, QLatin1Char('0'))
                                  .arg(mb, 0, 'f', 1));
}
