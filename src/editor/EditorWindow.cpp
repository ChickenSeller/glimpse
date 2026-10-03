#include "EditorWindow.h"
#include "ui_EditorWindow.h"

#include "AnnotatorWindow.h"
#include "ImageActions.h"

#include <QAction>
#include <QDir>
#include <QGuiApplication>
#include <QScreen>
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
    connect(ui->actionEdit, &QAction::triggered, this, &EditorWindow::edit);
    connect(ui->actionClose, &QAction::triggered, this, &QWidget::close);

    for (QAction *action : {ui->actionSaveAs, ui->actionCopy, ui->actionEdit})
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
    for (QAction *action : {ui->actionSaveAs, ui->actionCopy, ui->actionEdit}) {
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

void EditorWindow::setImage(const QImage &image)
{
    m_image = image;
    ui->canvas->setPixmap(QPixmap::fromImage(m_image));
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
