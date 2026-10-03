#include "EditorWindow.h"
#include "ui_EditorWindow.h"

#include <QAction>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>
#include <QScreen>
#include <QStandardPaths>
#include <QStatusBar>

EditorWindow::EditorWindow(const QImage &image, QWidget *parent)
    : QMainWindow(parent)
    , ui(std::make_unique<Ui::EditorWindow>())
    , m_image(image)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->canvas->setPixmap(QPixmap::fromImage(m_image));

    // actionClose is not on a toolbar, so register it on the window for its shortcut.
    addAction(ui->actionClose);
    connect(ui->actionSaveAs, &QAction::triggered, this, &EditorWindow::saveAs);
    connect(ui->actionCopy, &QAction::triggered, this, &EditorWindow::copyToClipboard);
    connect(ui->actionClose, &QAction::triggered, this, &QWidget::close);

    updateTexts();
    fitToScreen();
}

EditorWindow::~EditorWindow() = default;

void EditorWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        updateTexts();
    }
    QMainWindow::changeEvent(event);
}

void EditorWindow::updateTexts()
{
    setWindowTitle(tr("Glimpse - %1 × %2").arg(m_image.width()).arg(m_image.height()));
    statusBar()->showMessage(tr("%1 × %2 pixels").arg(m_image.width()).arg(m_image.height()));
}

void EditorWindow::saveAs()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString name = QStringLiteral("Glimpse_%1.png")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Capture"), QDir(dir).filePath(name),
        tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;BMP Image (*.bmp)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".png");

    if (!m_image.save(path)) {
        QMessageBox::warning(this, tr("Save Capture"), tr("Could not save the image to\n%1").arg(path));
        return;
    }
    statusBar()->showMessage(tr("Saved to %1").arg(QDir::toNativeSeparators(path)), 5000);
}

void EditorWindow::copyToClipboard()
{
    QGuiApplication::clipboard()->setImage(m_image);
    statusBar()->showMessage(tr("Copied to clipboard"), 3000);
}

void EditorWindow::fitToScreen()
{
    const QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect available = screen->availableGeometry();

    const QSizeF logical = QSizeF(m_image.size()) / m_image.devicePixelRatio();
    const QSize chrome(40, 120); // toolbar, status bar, scroll area frame
    const QSize wanted = (logical.toSize() + chrome)
                             .expandedTo(QSize(480, 320))
                             .boundedTo(available.size() * 0.85);
    resize(wanted);
    move(available.center() - rect().center());
}
