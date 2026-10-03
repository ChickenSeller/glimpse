#include "AnnotatorWindow.h"
#include "ui_AnnotatorWindow.h"

#include "ImageActions.h"

#include <kImageAnnotator/KImageAnnotator.h>

#include <QCloseEvent>
#include <QGuiApplication>
#include <QMessageBox>
#include <QScreen>
#include <QSettings>
#include <QGraphicsView>
#include <QSpinBox>
#include <QTimer>

namespace {

const QString kGeometryKey = QStringLiteral("editor/geometry");

} // namespace

AnnotatorWindow::AnnotatorWindow(const QImage &image, QWidget *parent)
    : QMainWindow(parent)
    , ui(std::make_unique<Ui::AnnotatorWindow>())
    , m_devicePixelRatio(image.devicePixelRatio())
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    m_annotator = new kImageAnnotator::KImageAnnotator;
    m_annotator->setTabBarAutoHide(true);
    m_annotator->setSmoothPathEnabled(true);
    // Persist each tool's color, width, font, shadow, fill, opacity... and the
    // selected tool (kImageAnnotator stores them in our QSettings).
    m_annotator->setSaveToolSelection(true);
    // Edit in device pixels: every annotation lands on exact image pixels.
    // kImageAnnotator fills its canvas white, on screen and in the result. A
    // transparent canvas keeps transparency and shows its checkerboard instead.
    if (ImageActions::hasTransparency(image))
        m_annotator->setCanvasColor(Qt::transparent);
    QImage pixels = image;
    pixels.setDevicePixelRatio(1.0);
    m_annotator->loadImage(QPixmap::fromImage(pixels));
    setCentralWidget(m_annotator);
    connect(m_annotator, &kImageAnnotator::KImageAnnotator::imageChanged, this, [this] { m_modified = true; });

    connect(ui->actionDone, &QAction::triggered, this, &AnnotatorWindow::done);
    connect(ui->actionSaveAs, &QAction::triggered, this, &AnnotatorWindow::saveAs);
    connect(ui->actionCopy, &QAction::triggered, this, &AnnotatorWindow::copyToClipboard);
    // Image-wide operations are modes of the annotator, not tools in its side bar.
    connect(ui->actionCrop, &QAction::triggered, m_annotator, &kImageAnnotator::KImageAnnotator::showCropper);
    connect(ui->actionCut, &QAction::triggered, m_annotator, &kImageAnnotator::KImageAnnotator::showCutter);
    connect(ui->actionResize, &QAction::triggered, m_annotator, &kImageAnnotator::KImageAnnotator::showScaler);
    connect(ui->actionRotate, &QAction::triggered, m_annotator, &kImageAnnotator::KImageAnnotator::showRotator);
    connect(ui->actionCanvas, &QAction::triggered, m_annotator,
            &kImageAnnotator::KImageAnnotator::showCanvasModifier);

    for (QAction *action : {ui->actionDone, ui->actionSaveAs, ui->actionCopy, ui->actionCrop, ui->actionCut,
                            ui->actionResize, ui->actionRotate, ui->actionCanvas})
        m_icons.add(action);
    m_icons.apply(palette().color(QPalette::ButtonText));
    updateToolTips();
    if (!restoreGeometry(QSettings().value(kGeometryKey).toByteArray()))
        fitToScreen();

    // Once shown on its screen, start at one image pixel per device pixel.
    QTimer::singleShot(0, this, &AnnotatorWindow::showPixelExact);
}

void AnnotatorWindow::showPixelExact()
{
    // The image is edited in device pixels (so exports keep full resolution),
    // which kImageAnnotator shows at 100% = one image pixel per *logical*
    // pixel; on a scaled display that resamples it (blur, jaggies). Scaling the
    // view by exactly 1 / devicePixelRatio maps it 1:1 to the screen.
    // kImageAnnotator has no public zoom API and rounds zoom to 10% steps, so
    // set its view's transform directly and only mirror the value in its
    // ZoomPicker (signals blocked, or it would re-apply the rounded value).
    const qreal dpr = devicePixelRatioF();
    if (qFuzzyCompare(dpr, 1.0))
        return;

    const auto views = m_annotator->findChildren<QGraphicsView *>();
    for (QGraphicsView *view : views) {
        if (qstrcmp(view->metaObject()->className(), "kImageAnnotator::AnnotationView") == 0 && view->isVisible()) {
            view->resetTransform();
            view->scale(1.0 / dpr, 1.0 / dpr);
            break;
        }
    }

    const auto widgets = m_annotator->findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        if (qstrcmp(widget->metaObject()->className(), "kImageAnnotator::ZoomPicker") != 0 || !widget->isVisible())
            continue;
        if (auto *box = widget->findChild<QSpinBox *>()) {
            const QSignalBlocker blocker(box);
            box->setValue(qRound(100.0 / dpr));
        }
        break;
    }
}

AnnotatorWindow::~AnnotatorWindow() = default;

void AnnotatorWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        updateToolTips();
    } else if (event->type() == QEvent::PaletteChange) {
        m_icons.apply(palette().color(QPalette::ButtonText));
    }
    QMainWindow::changeEvent(event);
}

void AnnotatorWindow::closeEvent(QCloseEvent *event)
{
    if (m_modified && !m_finished) {
        const auto answer = QMessageBox::question(
            this, windowTitle(), tr("Apply your edits to the capture before closing the editor?"),
            QMessageBox::Apply | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Apply);
        if (answer == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        if (answer == QMessageBox::Apply) {
            m_finished = true;
            emit finished(editedImage());
        }
    }
    QSettings().setValue(kGeometryKey, saveGeometry());
    event->accept();
}

QImage AnnotatorWindow::editedImage() const
{
    QImage image = m_annotator->image();
    image.setDevicePixelRatio(m_devicePixelRatio);
    return image;
}

void AnnotatorWindow::done()
{
    m_finished = true;
    emit finished(editedImage());
    close();
}

void AnnotatorWindow::saveAs()
{
    ImageActions::saveAs(this, editedImage());
}

void AnnotatorWindow::copyToClipboard()
{
    ImageActions::copyToClipboard(editedImage());
}

void AnnotatorWindow::updateToolTips()
{
    for (QAction *action : {ui->actionDone, ui->actionSaveAs, ui->actionCopy}) {
        QString text = action->toolTip();
        text.remove(QStringLiteral("..."));
        action->setToolTip(QStringLiteral("%1  (%2)").arg(text, action->shortcut().toString(QKeySequence::NativeText)));
    }
}

void AnnotatorWindow::fitToScreen()
{
    const QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect available = screen->availableGeometry();
    resize(available.size() * 0.85);
    move(available.center() - rect().center());
}
