#include "CaptureToolbar.h"
#include "ui_CaptureToolbar.h"

#include "TintedIcon.h"

#include <QCloseEvent>
#include <QSettings>
#include <QToolButton>

namespace {

const QString kGeometryKey = QStringLiteral("toolbar/geometry");

} // namespace

CaptureToolbar::CaptureToolbar(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::WindowStaysOnTopHint)
    , ui(std::make_unique<Ui::CaptureToolbar>())
{
    ui->setupUi(this);

    connect(ui->windowButton, &QToolButton::clicked, this, &CaptureToolbar::windowRequested);
    connect(ui->regionButton, &QToolButton::clicked, this, &CaptureToolbar::regionRequested);
    connect(ui->fullScreenButton, &QToolButton::clicked, this, &CaptureToolbar::fullScreenRequested);
    connect(ui->settingsButton, &QToolButton::clicked, this, &CaptureToolbar::settingsRequested);

    const auto buttons = findChildren<QToolButton *>();
    for (QToolButton *button : buttons) {
        m_sourceIcons.insert(button, button->icon());
        m_baseToolTips.insert(button, button->toolTip());
    }
    applyIconColor();

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

void CaptureToolbar::setHotkeyHints(const QKeySequence &window, const QKeySequence &region,
                                    const QKeySequence &fullScreen)
{
    m_hotkeys.insert(ui->windowButton, window);
    m_hotkeys.insert(ui->regionButton, region);
    m_hotkeys.insert(ui->fullScreenButton, fullScreen);
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
        updateToolTips();
    }
    QWidget::changeEvent(event);
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
