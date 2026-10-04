#include "CaptureToolbar.h"
#include "ui_CaptureToolbar.h"

#include "AppSettings.h"
#include "TintedIcon.h"
#include "capture/Platform.h"

#include <QActionGroup>
#include <QCloseEvent>
#include <QInputDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QToolButton>
#include <QWindow>


namespace {

const QString kGeometryKey = QStringLiteral("toolbar/geometry");

} // namespace

CaptureToolbar::CaptureToolbar(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint)
    , ui(std::make_unique<Ui::CaptureToolbar>())
{
    ui->setupUi(this);

    m_modeButtons = {
        {CaptureMode::Window, ui->windowButton},
        {CaptureMode::Region, ui->regionButton},
        {CaptureMode::Freehand, ui->freehandButton},
        {CaptureMode::FullScreen, ui->fullScreenButton},
        {CaptureMode::Recording, ui->recordButton},
        {CaptureMode::QrCode, ui->qrButton},
        {CaptureMode::Ocr, ui->ocrButton},
        {CaptureMode::Translate, ui->translateButton},
        {CaptureMode::ColorPicker, ui->colorButton},
        {CaptureMode::Crosshair, ui->crosshairButton},
        {CaptureMode::Scrolling, ui->scrollButton},
    };
    for (auto it = m_modeButtons.cbegin(); it != m_modeButtons.cend(); ++it) {
        const CaptureMode mode = it.key();
        connect(it.value(), &QToolButton::clicked, this, [this, mode] { emit captureRequested(mode); });
    }
    connect(ui->settingsButton, &QToolButton::clicked, this, &CaptureToolbar::settingsRequested);
    connect(ui->minimizeButton, &QToolButton::clicked, this, &CaptureToolbar::minimizeRequested);

    // No title bar: the grip moves the window. The window system does the
    // moving, which also works on Wayland, where windows cannot place themselves.
    ui->dragHandle->setCursor(Qt::SizeAllCursor);
    ui->dragHandle->installEventFilter(this);

    const auto buttons = findChildren<QToolButton *>();
    for (QToolButton *button : buttons) {
        m_sourceIcons.insert(button, button->icon());
        m_baseToolTips.insert(button, button->toolTip());
    }
    applyIconColor();
    applyPlatformLimits();
    if (kScrollingHidden)
        ui->scrollButton->hide();

    // The delay button has its own tooltip (with the current delay), not a hotkey hint.
    m_baseToolTips.remove(ui->delayButton);
    m_delayMenu = new QMenu(this);
    ui->delayButton->setMenu(m_delayMenu);
    setupDelayMenu();

    // Hidden for every capture: it must be gone at once, not fading out.
    Platform::disableWindowAnimations(this);

    // Wherever the user last left the bar; Qt moves it back on screen if that
    // monitor is gone. Without a saved value the window system places it.
    restoreGeometry(QSettings().value(kGeometryKey).toByteArray());
}

CaptureToolbar::~CaptureToolbar()
{
    // Quitting from the tray destroys the bar while it is still shown.
    if (isVisible())
        savePosition();
}

void CaptureToolbar::setHotkeyHints(const QHash<CaptureMode, QKeySequence> &hotkeys)
{
    m_hotkeys.clear();
    for (auto it = m_modeButtons.cbegin(); it != m_modeButtons.cend(); ++it)
        m_hotkeys.insert(it.value(), hotkeys.value(it.key()));
    updateToolTips();
}

void CaptureToolbar::updateToolTips()
{
    for (auto it = m_baseToolTips.cbegin(); it != m_baseToolTips.cend(); ++it) {
        const QKeySequence key = m_hotkeys.value(it.key());
        it.key()->setToolTip(key.isEmpty() ? it.value()
                                           : QStringLiteral("%1  (%2)").arg(it.value(),
                                                                            key.toString(QKeySequence::NativeText)));
    }
}

void CaptureToolbar::changeEvent(QEvent *event)
{
    // Follows light/dark theme switches, which arrive as a palette change.
    if (event->type() == QEvent::PaletteChange) {
        applyIconColor();
    } else if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        for (auto it = m_baseToolTips.begin(); it != m_baseToolTips.end(); ++it)
            it.value() = it.key()->toolTip();
        m_baseToolTips.remove(ui->delayButton);
        applyPlatformLimits();
        updateToolTips();
        setupDelayMenu();
    }
    QWidget::changeEvent(event);
}

bool CaptureToolbar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->dragHandle && event->type() == QEvent::MouseButtonPress
        && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
        if (QWindow *window = windowHandle())
            window->startSystemMove();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void CaptureToolbar::paintEvent(QPaintEvent *)
{
    // Without a frame the bar needs its own edge to stand out from what is behind it.
    QPainter painter(this);
    painter.fillRect(rect(), palette().window());
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void CaptureToolbar::applyPlatformLimits()
{
    for (auto it = m_modeButtons.cbegin(); it != m_modeButtons.cend(); ++it) {
        if (captureModeSupported(it.key()))
            continue;
        it.value()->setEnabled(false);
        m_baseToolTips[it.value()] += QLatin1Char('\n') + Platform::unsupportedHint();
    }
}

void CaptureToolbar::setupDelayMenu()
{
    // FastStone-style presets plus a custom value; the choice is persisted and
    // applies to every capture, however it is started.
    m_delayMenu->clear();
    delete m_delayGroup;
    m_delayGroup = new QActionGroup(this);
    const int current = AppSettings::captureDelay();
    bool matched = false;
    for (int seconds : {0, 1, 2, 3, 5, 10}) {
        QAction *action = m_delayMenu->addAction(seconds == 0 ? tr("No Delay") : tr("%n second(s)", nullptr, seconds));
        action->setCheckable(true);
        action->setChecked(seconds == current);
        matched |= seconds == current;
        m_delayGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, seconds] {
            AppSettings::setCaptureDelay(seconds);
            setupDelayMenu();
        });
        if (seconds == 0)
            m_delayMenu->addSeparator();
    }
    m_delayMenu->addSeparator();
    QAction *custom = m_delayMenu->addAction(matched ? tr("Custom...") : tr("Custom (%n s)...", nullptr, current));
    custom->setCheckable(true);
    custom->setChecked(!matched);
    m_delayGroup->addAction(custom);
    connect(custom, &QAction::triggered, this, &CaptureToolbar::chooseCustomDelay);
    updateDelayButton();
}

void CaptureToolbar::chooseCustomDelay()
{
    const int current = AppSettings::captureDelay();
    QInputDialog dialog(this);
    // The bar stays on top of everything; so must its dialog, or it opens behind it.
    dialog.setWindowFlag(Qt::WindowStaysOnTopHint);
    dialog.setWindowTitle(tr("Delay Before Capture"));
    dialog.setLabelText(tr("Seconds to wait before capturing:"));
    dialog.setInputMode(QInputDialog::IntInput);
    dialog.setIntRange(1, 60);
    dialog.setIntValue(current > 0 ? current : 3);
    if (dialog.exec() == QDialog::Accepted)
        AppSettings::setCaptureDelay(dialog.intValue());
    setupDelayMenu(); // also restores the checked item when canceled
}

void CaptureToolbar::updateDelayButton()
{
    const int delay = AppSettings::captureDelay();
    // With a delay set, the button says so at a glance.
    ui->delayButton->setText(delay > 0 ? tr("%1 s").arg(delay) : QString());
    ui->delayButton->setToolButtonStyle(delay > 0 ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);
    ui->delayButton->setToolTip(delay > 0 ? tr("Delay Before Capture: %n second(s)", nullptr, delay)
                                          : tr("Delay Before Capture: off"));
}

void CaptureToolbar::applyIconColor()
{
    const QColor color = palette().color(QPalette::ButtonText);
    for (auto it = m_sourceIcons.cbegin(); it != m_sourceIcons.cend(); ++it)
        it.key()->setIcon(tintedIcon(it.value(), color));
}

void CaptureToolbar::hideEvent(QHideEvent *event)
{
    // Covers closing the bar and hiding it for a capture.
    if (!event->spontaneous())
        savePosition();
    QWidget::hideEvent(event);
}

void CaptureToolbar::savePosition()
{
    QSettings().setValue(kGeometryKey, saveGeometry());
}

void CaptureToolbar::closeEvent(QCloseEvent *event)
{
    event->accept();
    emit closed();
}
