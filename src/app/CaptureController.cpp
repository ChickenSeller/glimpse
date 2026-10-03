#include "CaptureController.h"

#include "AppIcon.h"
#include "AppSettings.h"
#include "CaptureToolbar.h"
#include "capture/ScreenGrabber.h"
#include "editor/EditorWindow.h"
#include "hotkey/GlobalHotkeys.h"
#include "overlay/RegionSelector.h"
#include "settings/SettingsDialog.h"

#include <QApplication>
#include <QMenu>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QTimer>

namespace {

// Time for our own windows (toolbar, tray menu) to disappear before grabbing;
// compositors animate window hiding.
constexpr int kHideDelayMs = 300;
constexpr int kShortDelayMs = 150;

const char *captureLabel(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Window: return QT_TRANSLATE_NOOP("CaptureController", "Capture Window / Object");
    case CaptureMode::Region: return QT_TRANSLATE_NOOP("CaptureController", "Capture Rectangular Region");
    case CaptureMode::FullScreen: return QT_TRANSLATE_NOOP("CaptureController", "Capture Full Screen");
    }
    return "";
}

} // namespace

CaptureController::CaptureController(QObject *parent)
    : QObject(parent)
    , m_grabber(ScreenGrabber::create(this))
    , m_hotkeys(GlobalHotkeys::create(this))
    , m_toolbar(std::make_unique<CaptureToolbar>())
{
    connect(m_grabber, &ScreenGrabber::captured, this, &CaptureController::onSnapshot);
    connect(m_grabber, &ScreenGrabber::failed, this, &CaptureController::onGrabFailed);
    connect(m_grabber, &ScreenGrabber::canceled, this, &CaptureController::endCapture);

    connect(m_toolbar.get(), &CaptureToolbar::windowRequested, this, &CaptureController::captureWindow);
    connect(m_toolbar.get(), &CaptureToolbar::regionRequested, this, &CaptureController::captureRegion);
    connect(m_toolbar.get(), &CaptureToolbar::fullScreenRequested, this, &CaptureController::captureFullScreen);
    connect(m_toolbar.get(), &CaptureToolbar::settingsRequested, this, &CaptureController::showSettings);
    connect(m_toolbar.get(), &CaptureToolbar::closed, this, [this] {
        // Without a tray icon there would be no way back, so closing the bar quits.
        if (!m_tray)
            QApplication::quit();
    });

    connect(m_hotkeys, &GlobalHotkeys::activated, this, [this](int id) {
        beginCapture(static_cast<Mode>(id));
    });

    setupTray();
    // QApplication itself receives LanguageChange when a translator is (un)installed.
    qApp->installEventFilter(this);

    const QStringList taken = registerHotkeys();
    if (!taken.isEmpty() && m_tray) {
        m_tray->showMessage(tr("Hotkeys unavailable"),
                            tr("Already in use by another program: %1").arg(taken.join(QStringLiteral(", "))),
                            QSystemTrayIcon::Warning);
    }
}

CaptureController::~CaptureController() = default;

void CaptureController::showToolbar()
{
    m_toolbar->show();
    m_toolbar->raise();
    m_toolbar->activateWindow();
}

void CaptureController::showSettings()
{
    if (m_settings) {
        m_settings->raise();
        m_settings->activateWindow();
        return;
    }

    // Paused so the hotkey fields can record combinations we currently own.
    m_hotkeys->clear();

    m_settings = new SettingsDialog;
    m_settings->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_settings, &QDialog::finished, this, [this](int result) {
        const QStringList taken = registerHotkeys();
        if (result == QDialog::Accepted && !taken.isEmpty()) {
            QMessageBox::warning(nullptr, tr("Hotkeys unavailable"),
                                 tr("These hotkeys are already in use by another program and will not work:\n%1")
                                     .arg(taken.join(QStringLiteral(", "))));
        }
    });
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}

void CaptureController::captureFullScreen()
{
    beginCapture(Mode::FullScreen);
}

void CaptureController::captureRegion()
{
    beginCapture(Mode::Region);
}

void CaptureController::captureWindow()
{
    beginCapture(Mode::Window);
}

void CaptureController::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_trayMenu = std::make_unique<QMenu>();
    for (CaptureMode mode : kAllCaptureModes) {
        QAction *item = m_trayMenu->addAction(QString(), this, [this, mode] { beginCapture(mode); });
        // The shortcut is only a hint (set in registerHotkeys); the global hotkey does the work.
        item->setShortcutVisibleInContextMenu(true);
        m_trayCaptureActions.insert(mode, item);
    }
    m_trayMenu->addSeparator();
    m_trayShowToolbar = m_trayMenu->addAction(QString(), this, &CaptureController::showToolbar);
    m_traySettings = m_trayMenu->addAction(QString(), this, &CaptureController::showSettings);
    m_trayMenu->addSeparator();
    m_trayExit = m_trayMenu->addAction(QString(), qApp, &QApplication::quit);

    m_tray = new QSystemTrayIcon(appIcon(), this);
    m_tray->setToolTip(QStringLiteral("Glimpse"));
    m_tray->setContextMenu(m_trayMenu.get());
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            showToolbar();
    });
    m_tray->show();
    retranslate();
}

void CaptureController::retranslate()
{
    for (auto it = m_trayCaptureActions.cbegin(); it != m_trayCaptureActions.cend(); ++it)
        it.value()->setText(tr(captureLabel(it.key())));
    if (m_trayShowToolbar) {
        m_trayShowToolbar->setText(tr("Show Toolbar"));
        m_traySettings->setText(tr("Settings..."));
        m_trayExit->setText(tr("Exit"));
    }
}

bool CaptureController::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qApp && event->type() == QEvent::LanguageChange)
        retranslate();
    return QObject::eventFilter(watched, event);
}

QStringList CaptureController::registerHotkeys()
{
    m_hotkeys->clear();

    QStringList taken;
    QHash<Mode, QKeySequence> active;
    for (CaptureMode mode : kAllCaptureModes) {
        const QKeySequence key = AppSettings::hotkey(mode);
        if (key.isEmpty())
            continue;
        if (m_hotkeys->add(int(mode), key))
            active.insert(mode, key);
        else
            taken << key.toString(QKeySequence::NativeText);
    }

    // Hints show only hotkeys that actually work.
    m_toolbar->setHotkeyHints(active.value(Mode::Window), active.value(Mode::Region),
                              active.value(Mode::FullScreen));
    for (auto it = m_trayCaptureActions.cbegin(); it != m_trayCaptureActions.cend(); ++it)
        it.value()->setShortcut(active.value(it.key()));

    // Without a backend nothing registers; that is not a conflict worth reporting.
    return m_hotkeys->isSupported() ? taken : QStringList();
}

void CaptureController::beginCapture(Mode mode)
{
    if (m_busy)
        return;
    m_busy = true;
    m_mode = mode;

    m_restoreToolbar = m_toolbar->isVisible();
    if (m_restoreToolbar)
        m_toolbar->hide();

    QTimer::singleShot(m_restoreToolbar ? kHideDelayMs : kShortDelayMs, m_grabber, &ScreenGrabber::grab);
}

void CaptureController::onSnapshot(const DesktopSnapshot &snapshot)
{
    if (m_mode == Mode::FullScreen) {
        openEditor(snapshot.crop(snapshot.virtualGeometry()));
        endCapture();
        return;
    }

    const auto selectorMode = m_mode == Mode::Window ? RegionSelector::Mode::Window
                                                     : RegionSelector::Mode::Region;
    auto *selector = new RegionSelector(snapshot, selectorMode, this);
    connect(selector, &RegionSelector::selected, this, [this, selector](const QRect &rect) {
        const QImage image = selector->snapshot().crop(rect);
        selector->deleteLater();
        openEditor(image);
        endCapture();
    });
    connect(selector, &RegionSelector::canceled, this, [this, selector] {
        selector->deleteLater();
        endCapture();
    });
    selector->start();
}

void CaptureController::onGrabFailed(const QString &message)
{
    endCapture();
    if (m_tray)
        m_tray->showMessage(tr("Capture failed"), message, QSystemTrayIcon::Warning);
    else
        QMessageBox::warning(nullptr, tr("Capture failed"), message);
}

void CaptureController::endCapture()
{
    m_busy = false;
    if (m_restoreToolbar) {
        m_restoreToolbar = false;
        m_toolbar->show();
    }
}

void CaptureController::openEditor(const QImage &image)
{
    if (image.isNull())
        return;
    auto *editor = new EditorWindow(image);
    editor->show();
    editor->raise();
    editor->activateWindow();
}
