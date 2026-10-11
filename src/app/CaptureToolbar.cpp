#include "CaptureToolbar.h"
#include "ui_CaptureToolbar.h"

#include "AppSettings.h"
#include "TintedIcon.h"
#include "capture/Platform.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QInputDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QSettings>
#include <QTimer>
#include <QToolButton>
#include <QVariantAnimation>
#include <QWindow>

#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#endif


namespace {

const QString kGeometryKey = QStringLiteral("toolbar/geometry");
const QString kDockEdgeKey = QStringLiteral("toolbar/dockEdge");
const QString kDockScreenKey = QStringLiteral("toolbar/dockScreen");
const QString kDockPositionKey = QStringLiteral("toolbar/dockPosition");

constexpr int kSlideInDelayMs = 1000; // after the pointer leaves a docked bar
constexpr int kSlideMs = 500;         // a whole slide in or out, at the top or bottom
constexpr int kSlideSidewaysMs = 300; // at the left or right: the whole length of the bar
constexpr int kDockStrip = 4;         // what stays on screen, slid in
constexpr int kDragSettleMs = 400;    // no more moves: the drag is over (where unreported)

// Native Wayland does not tell windows where they are.
bool dockingSupported()
{
    return Platform::windowsCanPlaceThemselves();
}

} // namespace

CaptureToolbar::CaptureToolbar(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint)
    , ui(std::make_unique<Ui::CaptureToolbar>())
{
    ui->setupUi(this);

    m_modeButtons = {
        {CaptureMode::Window, ui->windowButton},
        {CaptureMode::Freehand, ui->freehandButton},
        {CaptureMode::FullScreen, ui->fullScreenButton},
        {CaptureMode::Pin, ui->pinButton},
        {CaptureMode::Recording, ui->recordButton},
        {CaptureMode::QrCode, ui->qrButton},
        {CaptureMode::Ocr, ui->ocrButton},
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

    // The delay button has its own tooltip (with the current delay), not a hotkey hint.
    m_baseToolTips.remove(ui->delayButton);
    m_delayMenu = new QMenu(this);
    ui->delayButton->setMenu(m_delayMenu);
    setupDelayMenu();

    // Hidden for every capture: it must be gone at once, not fading out.
    Platform::disableWindowAnimations(this);

    m_slide = new QVariantAnimation(this);
    m_slide->setEasingCurve(QEasingCurve::InOutCubic); // eases off at both ends
    connect(m_slide, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) { setHiddenAmount(value.toReal()); });
    m_slideInTimer = new QTimer(this);
    m_slideInTimer->setSingleShot(true);
    m_slideInTimer->setInterval(kSlideInDelayMs);
    connect(m_slideInTimer, &QTimer::timeout, this, &CaptureToolbar::slideInIfIdle);
    m_dragEndTimer = new QTimer(this);
    m_dragEndTimer->setSingleShot(true);
    m_dragEndTimer->setInterval(kDragSettleMs);
    connect(m_dragEndTimer, &QTimer::timeout, this, &CaptureToolbar::endDrag);
    connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen *screen) {
        if (screen == m_dockScreen) {
            undock();
            keepOnScreen();
        }
    });

    applyLayoutSettings();

    // Wherever the user last left the bar; Qt moves it back on screen if that
    // monitor is gone. Without a saved value the window system places it.
    restoreGeometry(QSettings().value(kGeometryKey).toByteArray());
    resize(ui->panel->size()); // saved while slid in, it was a strip
    keepOnScreen();            // at the size the settings ask for
    restoreDock();

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

void CaptureToolbar::applyLayoutSettings()
{
    QHash<QString, QToolButton *> buttons;
    for (auto it = m_modeButtons.cbegin(); it != m_modeButtons.cend(); ++it)
        buttons.insert(AppSettings::toolbarItemId(it.key()), it.value());
    buttons.insert(AppSettings::kDelayItemId, ui->delayButton);

    // Right after the drag grip, in the chosen order.
    int index = ui->layout->indexOf(ui->dragHandle) + 1;
    const QList<AppSettings::ToolbarItem> items = AppSettings::toolbarItems();
    for (const AppSettings::ToolbarItem &item : items) {
        QToolButton *button = buttons.value(item.id);
        if (!button)
            continue;
        ui->layout->removeWidget(button);
        ui->layout->insertWidget(index++, button);
        button->setVisible(item.visible);
    }

    // Icons, and the gaps around them, at the chosen size.
    const qreal scale = AppSettings::toolbarScale() / 100.0;
    const int size = AppSettings::toolbarIconSize();
    const auto all = findChildren<QToolButton *>();
    for (QToolButton *button : all)
        button->setIconSize(QSize(size, size));
    if (m_baseSpacing < 0) {
        m_baseSpacing = ui->layout->spacing();
        m_baseMargins = ui->layout->contentsMargins();
    }
    ui->layout->setSpacing(qRound(m_baseSpacing * scale));
    ui->layout->setContentsMargins(qRound(m_baseMargins.left() * scale), qRound(m_baseMargins.top() * scale),
                                   qRound(m_baseMargins.right() * scale), qRound(m_baseMargins.bottom() * scale));
    // The window follows the panel: it fits the new size, and shrinks when
    // buttons were hidden.
    ui->layout->activate();
    ui->panel->adjustSize();
    if (m_dockEdge != DockEdge::None && m_dockScreen) {
        dockTo(m_dockScreen, m_dockEdge, m_dockPosition);
    } else {
        resize(ui->panel->size());
        keepOnScreen();
    }
}

void CaptureToolbar::reveal()
{
    if (m_dockEdge == DockEdge::None)
        return;
    slideTo(0);
    scheduleSlideIn();
}

// Grown, the bar may reach past the edge of its screen: move it back in.
void CaptureToolbar::keepOnScreen()
{
    if (m_dockEdge != DockEdge::None)
        return; // docked, it is placed on its screen already
    if (const QScreen *screen = this->screen()) {
        const QRect available = screen->availableGeometry();
        QRect frame = frameGeometry();
        if (!available.contains(frame)) {
            frame.moveRight(std::min(frame.right(), available.right()));
            frame.moveBottom(std::min(frame.bottom(), available.bottom()));
            frame.moveLeft(std::max(frame.left(), available.left()));
            frame.moveTop(std::max(frame.top(), available.top()));
            move(frame.topLeft());
        }
    }
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
        beginDrag();
        QWindow *window = windowHandle();
        if (!window || !window->startSystemMove())
            m_dragging = false;
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void CaptureToolbar::paintEvent(QPaintEvent *)
{
    // Without a frame the bar needs its own edge to stand out from what is behind it.
    // Around the panel, which slides past the window's edge when docked.
    QPainter painter(this);
    painter.fillRect(rect(), palette().window());
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawRect(ui->panel->geometry().adjusted(0, 0, -1, -1));
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
    QSettings settings;
    settings.setValue(kGeometryKey, saveGeometry());
    if (m_dockEdge != DockEdge::None && m_dockScreen) {
        settings.setValue(kDockEdgeKey, int(m_dockEdge));
        settings.setValue(kDockScreenKey, m_dockScreen->name());
        settings.setValue(kDockPositionKey, m_dockPosition);
    } else {
        settings.remove(kDockEdgeKey);
        settings.remove(kDockScreenKey);
        settings.remove(kDockPositionKey);
    }
}

void CaptureToolbar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // Back after a capture: as it was, slid in or out.
    if (m_dockEdge != DockEdge::None) {
        setHiddenAmount(m_hiddenAmount);
        if (m_hiddenAmount == 0)
            scheduleSlideIn();
    }
}

void CaptureToolbar::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    m_slideInTimer->stop();
    if (m_dockEdge != DockEdge::None && !m_dragging)
        slideTo(0);
}

void CaptureToolbar::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    scheduleSlideIn();
}

void CaptureToolbar::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
#ifndef Q_OS_WIN
    // X11 does not say when a window manager move ends: it has once the bar
    // stops moving for a moment.
    if (m_dragging)
        m_dragEndTimer->start();
#endif
}

bool CaptureToolbar::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    // The move started by startSystemMove() ends with the button released.
    if (m_dragging && eventType == "windows_generic_MSG"
        && static_cast<MSG *>(message)->message == WM_EXITSIZEMOVE)
        QTimer::singleShot(0, this, &CaptureToolbar::endDrag);
#endif
    return QWidget::nativeEvent(eventType, message, result);
}

void CaptureToolbar::beginDrag()
{
    m_dragging = true;
    m_slideInTimer->stop();
    m_dragEndTimer->stop();
    undock(); // whole again, to be moved
}

// Dropped against an edge of the screen under the pointer: docked there.
// The pointer, not the bar, picks the screen: at an edge between two screens
// the bar reaches into the other one.
void CaptureToolbar::endDrag()
{
    if (!m_dragging)
        return;
    m_dragging = false;
    m_dragEndTimer->stop();
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = this->screen();
    if (!dockingSupported() || !screen) {
        keepOnScreen();
        return;
    }
    const QRect area = screen->availableGeometry();
    const QRect frame = frameGeometry();
    DockEdge edge = DockEdge::None;
    if (frame.left() <= area.left())
        edge = DockEdge::Left;
    else if (frame.right() >= area.right())
        edge = DockEdge::Right;
    else if (frame.top() <= area.top())
        edge = DockEdge::Top;
    else if (frame.bottom() >= area.bottom())
        edge = DockEdge::Bottom;
    if (edge == DockEdge::None) {
        keepOnScreen();
        return;
    }
    dockTo(screen, edge, frame.topLeft());
    if (!frameGeometry().contains(QCursor::pos()))
        scheduleSlideIn();
}

// Against the edge, wholly on the screen, at the slide it had.
void CaptureToolbar::dockTo(QScreen *screen, DockEdge edge, QPoint position)
{
    m_dockScreen = screen;
    m_dockEdge = edge;
    const QRect area = screen->availableGeometry();
    const QSize size = ui->panel->size();
    int x = std::clamp(position.x(), area.left(), std::max(area.left(), area.right() - size.width() + 1));
    int y = std::clamp(position.y(), area.top(), std::max(area.top(), area.bottom() - size.height() + 1));
    switch (edge) {
    case DockEdge::Left: x = area.left(); break;
    case DockEdge::Right: x = area.right() - size.width() + 1; break;
    case DockEdge::Top: y = area.top(); break;
    case DockEdge::Bottom: y = area.bottom() - size.height() + 1; break;
    case DockEdge::None: break;
    }
    m_dockPosition = QPoint(x, y);
    setHiddenAmount(m_hiddenAmount);
}

void CaptureToolbar::undock()
{
    m_slide->stop();
    if (m_dockEdge != DockEdge::None)
        setHiddenAmount(0);
    m_dockEdge = DockEdge::None;
    m_dockScreen = nullptr;
    m_hiddenAmount = 0;
    ui->panel->move(0, 0);
}

void CaptureToolbar::restoreDock()
{
    if (!dockingSupported())
        return;
    QSettings settings;
    const int edge = settings.value(kDockEdgeKey, 0).toInt();
    if (edge <= int(DockEdge::None) || edge > int(DockEdge::Bottom))
        return;
    const QString name = settings.value(kDockScreenKey).toString();
    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (screen->name() == name) {
            m_hiddenAmount = 1; // starts slid in, out of the way
            dockTo(screen, DockEdge(edge), settings.value(kDockPositionKey).toPoint());
            return;
        }
    }
}

// The window shrinks toward the edge down to a strip; the panel inside moves
// so that the part nearest the edge is what stays, as if the bar slid in.
void CaptureToolbar::setHiddenAmount(qreal amount)
{
    m_hiddenAmount = amount;
    if (m_dockEdge == DockEdge::None)
        return;
    const QSize full = ui->panel->size();
    QRect frame(m_dockPosition, full);
    QPoint panel(0, 0);
    if (m_dockEdge == DockEdge::Left || m_dockEdge == DockEdge::Right) {
        const int shown = std::max(kDockStrip, qRound(full.width() - (full.width() - kDockStrip) * amount));
        frame.setWidth(shown);
        if (m_dockEdge == DockEdge::Right)
            frame.moveRight(m_dockPosition.x() + full.width() - 1);
        else
            panel.setX(shown - full.width());
    } else {
        const int shown = std::max(kDockStrip, qRound(full.height() - (full.height() - kDockStrip) * amount));
        frame.setHeight(shown);
        if (m_dockEdge == DockEdge::Bottom)
            frame.moveBottom(m_dockPosition.y() + full.height() - 1);
        else
            panel.setY(shown - full.height());
    }
    ui->panel->move(panel);
    setGeometry(frame);
    update();
}

void CaptureToolbar::slideTo(qreal amount)
{
    if (m_dockEdge == DockEdge::None)
        return;
    m_slide->stop();
    const qreal from = m_hiddenAmount;
    if (qFuzzyCompare(1 + from, 1 + amount))
        return;
    {
        // A finished animation stays at its end: changing its values would
        // report the end value right away and jump the bar there.
        const QSignalBlocker blocker(m_slide);
        const bool sideways = m_dockEdge == DockEdge::Left || m_dockEdge == DockEdge::Right;
        m_slide->setDuration(std::max(1, qRound((sideways ? kSlideSidewaysMs : kSlideMs) * std::abs(amount - from))));
        m_slide->setStartValue(from);
        m_slide->setEndValue(amount);
    }
    m_slide->start();
}

void CaptureToolbar::scheduleSlideIn()
{
    if (m_dockEdge != DockEdge::None && !m_dragging && isVisible())
        m_slideInTimer->start();
}

// Not while the pointer is on the bar, nor while its menu or a dialog of it is open.
void CaptureToolbar::slideInIfIdle()
{
    if (m_dockEdge == DockEdge::None || m_dragging)
        return;
    if (frameGeometry().contains(QCursor::pos()) || QApplication::activePopupWidget()
        || QApplication::activeModalWidget()) {
        m_slideInTimer->start();
        return;
    }
    slideTo(1);
}

void CaptureToolbar::closeEvent(QCloseEvent *event)
{
    event->accept();
    emit closed();
}
