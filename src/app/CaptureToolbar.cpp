#include "CaptureToolbar.h"
#include "ui_CaptureToolbar.h"

#include "TintedIcon.h"
#include "capture/Platform.h"

#include <QCloseEvent>
#include <QSettings>
#include <QToolButton>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace {

const QString kGeometryKey = QStringLiteral("toolbar/geometry");

} // namespace

CaptureToolbar::CaptureToolbar(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::WindowStaysOnTopHint)
    , ui(std::make_unique<Ui::CaptureToolbar>())
{
    ui->setupUi(this);

    m_modeButtons = {
        {CaptureMode::Window, ui->windowButton},
        {CaptureMode::Region, ui->regionButton},
        {CaptureMode::FullScreen, ui->fullScreenButton},
        {CaptureMode::QrCode, ui->qrButton},
        {CaptureMode::Ocr, ui->ocrButton},
    };
    for (auto it = m_modeButtons.cbegin(); it != m_modeButtons.cend(); ++it) {
        const CaptureMode mode = it.key();
        connect(it.value(), &QToolButton::clicked, this, [this, mode] { emit captureRequested(mode); });
    }
    connect(ui->settingsButton, &QToolButton::clicked, this, &CaptureToolbar::settingsRequested);

    const auto buttons = findChildren<QToolButton *>();
    for (QToolButton *button : buttons) {
        m_sourceIcons.insert(button, button->icon());
        m_baseToolTips.insert(button, button->toolTip());
    }
    applyIconColor();
    applyPlatformLimits();

#ifdef Q_OS_WIN
    // Windows fades hidden windows out; a capture taken right after hiding the
    // bar would still show it. Without the transition it disappears at once.
    const BOOL disable = TRUE;
    DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()), DWMWA_TRANSITIONS_FORCEDISABLED, &disable,
                          sizeof(disable));
#endif

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
        applyPlatformLimits();
        updateToolTips();
    }
    QWidget::changeEvent(event);
}

void CaptureToolbar::applyPlatformLimits()
{
    if (!Platform::supportsWindowPicking()) {
        ui->windowButton->setEnabled(false);
        m_baseToolTips[ui->windowButton] += QLatin1Char('\n') + Platform::unsupportedHint();
    }
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
