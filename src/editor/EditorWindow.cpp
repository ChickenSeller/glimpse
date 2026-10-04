#include "EditorWindow.h"
#include "ui_EditorWindow.h"

#include "AnnotatorWindow.h"
#include "ImageActions.h"
#include "pin/PinWindow.h"

#include <QAction>
#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QStatusBar>

namespace {

// The capture as shown: transparent parts (e.g. outside a freehand outline)
// over the usual light checkerboard, so they cannot be mistaken for a color.
QPixmap previewPixmap(const QImage &image)
{
    if (!image.hasAlphaChannel())
        return QPixmap::fromImage(image);

    const qreal dpr = image.devicePixelRatio();
    const int cell = qRound(8 * dpr);
    QImage preview(image.size(), QImage::Format_ARGB32_Premultiplied);
    preview.fill(Qt::white);
    QPainter painter(&preview);
    for (int y = 0; y < preview.height(); y += cell) {
        for (int x = (y / cell) % 2 * cell; x < preview.width(); x += 2 * cell)
            painter.fillRect(x, y, cell, cell, QColor(0xcc, 0xcc, 0xcc));
    }
    QImage source = image;
    source.setDevicePixelRatio(1); // paint pixel for pixel
    painter.drawImage(0, 0, source);
    painter.end();
    preview.setDevicePixelRatio(dpr);
    return QPixmap::fromImage(preview);
}

} // namespace

EditorWindow::EditorWindow(const QImage &image, QWidget *parent)
    : QMainWindow(parent)
    , ui(std::make_unique<Ui::EditorWindow>())
    , m_image(image)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->canvas->setPixmap(previewPixmap(m_image));

    // actionClose is not on a toolbar, so register it on the window for its shortcut.
    addAction(ui->actionClose);
    connect(ui->actionSaveAs, &QAction::triggered, this, &EditorWindow::saveAs);
    connect(ui->actionSaveCopyPath, &QAction::triggered, this, &EditorWindow::saveAndCopyPath);
    connect(ui->actionCopy, &QAction::triggered, this, &EditorWindow::copyToClipboard);
    connect(ui->actionEdit, &QAction::triggered, this, &EditorWindow::edit);
    connect(ui->actionPin, &QAction::triggered, this, &EditorWindow::pinToScreen);
    connect(ui->actionClose, &QAction::triggered, this, &QWidget::close);

    for (QAction *action : {ui->actionSaveAs, ui->actionSaveCopyPath, ui->actionCopy, ui->actionEdit, ui->actionPin})
        m_icons.add(action);
    m_icons.apply(palette().color(QPalette::ButtonText));

    updateToolTips();
    updateTexts();
    fitToScreen();
}

EditorWindow::~EditorWindow() = default;

void EditorWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        updateToolTips();
        updateTexts();
    } else if (event->type() == QEvent::PaletteChange) {
        m_icons.apply(palette().color(QPalette::ButtonText));
    }
    QMainWindow::changeEvent(event);
}

void EditorWindow::updateTexts()
{
    setWindowTitle(tr("Glimpse - %1 × %2").arg(m_image.width()).arg(m_image.height()));
    statusBar()->showMessage(tr("%1 × %2 pixels").arg(m_image.width()).arg(m_image.height()));
}

void EditorWindow::updateToolTips()
{
    // Icon-only buttons: name the shortcut in the tooltip (as set by the .ui).
    for (QAction *action : {ui->actionSaveAs, ui->actionSaveCopyPath, ui->actionCopy, ui->actionEdit, ui->actionPin}) {
        QString text = action->toolTip();
        text.remove(QStringLiteral("..."));
        action->setToolTip(QStringLiteral("%1  (%2)").arg(text, action->shortcut().toString(QKeySequence::NativeText)));
    }
}

void EditorWindow::saveAs()
{
    const QString path = ImageActions::saveAs(this, m_image);
    if (!path.isEmpty())
        statusBar()->showMessage(tr("Saved to %1").arg(QDir::toNativeSeparators(path)), 5000);
}

void EditorWindow::saveAndCopyPath()
{
    // For pasting the file into chats and tools that take a path (e.g. Claude Code).
    const QString path = QDir::toNativeSeparators(ImageActions::saveAs(this, m_image));
    if (path.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(path);
    statusBar()->showMessage(tr("Saved; path copied: %1").arg(path), 5000);
}

void EditorWindow::copyToClipboard()
{
    ImageActions::copyToClipboard(m_image);
    statusBar()->showMessage(tr("Copied to clipboard"), 3000);
}

void EditorWindow::edit()
{
    if (m_annotator) {
        m_annotator->raise();
        m_annotator->activateWindow();
        return;
    }
    m_annotator = new AnnotatorWindow(m_image);
    connect(m_annotator, &AnnotatorWindow::finished, this, &EditorWindow::setImage);
    // Closing the capture closes its editor too.
    connect(this, &QObject::destroyed, m_annotator, &QWidget::close);
    m_annotator->show();
    m_annotator->raise();
    m_annotator->activateWindow();
}

void EditorWindow::pinToScreen()
{
    // The pin takes over: it can copy, save and reopen the editor itself.
    auto *pin = new PinWindow(m_image);
    pin->show();
    pin->raise();
    pin->activateWindow();
    close();
}

void EditorWindow::setImage(const QImage &image)
{
    m_image = image;
    ui->canvas->setPixmap(previewPixmap(m_image));
    updateTexts();
    raise();
    activateWindow();
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
