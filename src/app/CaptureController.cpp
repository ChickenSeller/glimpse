#include "CaptureController.h"

#include "AppIcon.h"
#include "AppSettings.h"
#include "CaptureToolbar.h"
#include "DelayCountdown.h"
#include "capture/Platform.h"
#include "capture/ScrollCaptureSession.h"
#include "capture/ScreenGrabber.h"
#include "editor/EditorWindow.h"
#include "hotkey/GlobalHotkeys.h"
#include "overlay/RegionSelector.h"
#include "qr/BarcodeScanner.h"
#include "qr/QrResultDialog.h"
#include "ocr/ModelStore.h"
#include "ocr/OcrResultDialog.h"
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
    case CaptureMode::QrCode: return QT_TRANSLATE_NOOP("CaptureController", "Scan QR Code");
    case CaptureMode::Ocr: return QT_TRANSLATE_NOOP("CaptureController", "Recognize Text");
    case CaptureMode::Scrolling: return QT_TRANSLATE_NOOP("CaptureController", "Scrolling Capture");
    case CaptureMode::Freehand: return QT_TRANSLATE_NOOP("CaptureController", "Capture Freehand Region");
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

    connect(m_toolbar.get(), &CaptureToolbar::captureRequested, this, &CaptureController::beginCapture);
    connect(m_toolbar.get(), &CaptureToolbar::settingsRequested, this, &CaptureController::showSettings);
    connect(m_toolbar.get(), &CaptureToolbar::closed, this, [this] {
        // Without a tray icon there would be no way back, so closing the bar quits.
        if (!m_tray)
            QApplication::quit();
    });
    connect(m_toolbar.get(), &CaptureToolbar::minimizeRequested, this, [this] {
        // To the tray, like FastStone; the tray icon or a hotkey brings it back.
        // A tool window has no taskbar button, so without a tray it can only shrink.
        if (m_tray)
            m_toolbar->hide();
        else
            m_toolbar->showMinimized();
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
    m_toolbar->showNormal(); // also undoes a minimize
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
    connect(m_settings, &SettingsDialog::applied, this, [this] {
        // Register right away to report conflicts while the user can still fix
        // them, then pause again until the dialog closes.
        const QStringList taken = registerHotkeys();
        m_hotkeys->clear();
        if (!taken.isEmpty()) {
            QMessageBox::warning(m_settings, tr("Hotkeys unavailable"),
                                 tr("These hotkeys are already in use by another program and will not work:\n%1")
                                     .arg(taken.join(QStringLiteral(", "))));
        }
    });
    connect(m_settings, &QDialog::finished, this, [this] { registerHotkeys(); });
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
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
        item->setEnabled(captureModeSupported(mode));
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
    m_toolbar->setHotkeyHints(active);
    for (auto it = m_trayCaptureActions.cbegin(); it != m_trayCaptureActions.cend(); ++it)
        it.value()->setShortcut(active.value(it.key()));

    // Without a backend nothing registers; that is not a conflict worth reporting.
    return m_hotkeys->isSupported() ? taken : QStringList();
}

void CaptureController::beginCapture(Mode mode)
{
    if (m_busy || !captureModeSupported(mode))
        return;
    m_busy = true;
    m_mode = mode;

    // Hidden unless the user wants it in the picture.
    m_restoreToolbar = m_toolbar->isVisible() && !AppSettings::captureIncludesToolbar();
    if (m_restoreToolbar)
        m_toolbar->hide();
    m_grabber->setIncludeCursor(AppSettings::captureIncludesCursor() && Platform::supportsCursorCapture());

    m_pendingRect.reset();
    m_pendingShape.clear();

    // Full screen has nothing to choose, so its delay comes first. The other
    // modes freeze the screen for choosing the area right away and count down
    // afterwards (see onSnapshot), like FastStone.
    if (mode == Mode::FullScreen && AppSettings::captureDelay() > 0) {
        afterDelay([this] { m_grabber->grab(); });
        return;
    }
    QTimer::singleShot(m_restoreToolbar ? kHideDelayMs : kShortDelayMs, m_grabber, &ScreenGrabber::grab);
}

void CaptureController::afterDelay(std::function<void()> then)
{
    const int delay = AppSettings::captureDelay();
    if (delay <= 0) {
        then();
        return;
    }
    // The countdown hides itself before finishing; the short pause lets it
    // vanish from the screen before the grab.
    auto *countdown = new DelayCountdown(delay);
    connect(countdown, &DelayCountdown::finished, this, [this, countdown, then] {
        countdown->deleteLater();
        QTimer::singleShot(kShortDelayMs, this, then);
    });
    connect(countdown, &DelayCountdown::canceled, this, [this, countdown] {
        countdown->deleteLater();
        m_pendingRect.reset();
        m_pendingShape.clear();
        endCapture();
    });
    countdown->start();
}

void CaptureController::deliver(const QImage &image)
{
    endCapture();
    if (m_mode == Mode::QrCode)
        showQrResult(image);
    else if (m_mode == Mode::Ocr)
        showOcrResult(image);
    else
        openEditor(image);
}

void CaptureController::onSnapshot(const DesktopSnapshot &snapshot)
{
    // The live grab after a delay: the area was chosen before the countdown.
    if (m_pendingRect) {
        const QRect rect = *m_pendingRect;
        const QPainterPath shape = m_pendingShape;
        m_pendingRect.reset();
        m_pendingShape.clear();
        deliver(shape.isEmpty() ? snapshot.crop(rect) : snapshot.crop(shape, AppSettings::freehandFill()));
        return;
    }
    if (m_mode == Mode::FullScreen) {
        deliver(snapshot.crop(snapshot.virtualGeometry()));
        return;
    }

    // Scrolling capture picks its area like Window / Object: hover a
    // scrollable control and click, or drag a rectangle.
    const auto selectorMode = m_mode == Mode::Window || m_mode == Mode::Scrolling ? RegionSelector::Mode::Window
                              : m_mode == Mode::Freehand                          ? RegionSelector::Mode::Freehand
                                                                                  : RegionSelector::Mode::Region;
    auto *selector = new RegionSelector(snapshot, selectorMode, this);
    connect(selector, &RegionSelector::selected, this, [this, selector](const QRect &rect) {
        if (m_mode == Mode::Scrolling) {
            selector->deleteLater();
            startScrollingCapture(rect);
            return;
        }
        selector->deleteLater();
        const QPainterPath shape = selector->shape();
        if (AppSettings::captureDelay() > 0) {
            // Area first, then the countdown, then a fresh grab of that area:
            // menus or tooltips opened meanwhile end up in the capture.
            afterDelay([this, rect, shape] {
                m_pendingRect = rect;
                m_pendingShape = shape;
                m_grabber->grab();
            });
            return;
        }
        deliver(shape.isEmpty() ? selector->snapshot().crop(rect)
                                : selector->snapshot().crop(shape, AppSettings::freehandFill()));
    });
    connect(selector, &RegionSelector::canceled, this, [this, selector] {
        selector->deleteLater();
        endCapture();
    });
    selector->start();
}

void CaptureController::startScrollingCapture(const QRect &rect)
{
    // Still "busy": the toolbar stays hidden while the live screen is grabbed.
    auto *session = new ScrollCaptureSession(rect, this);
    connect(session, &ScrollCaptureSession::finished, this, [this, session](const QImage &image) {
        session->deleteLater();
        endCapture();
        openEditor(image);
    });
    session->start();
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

void CaptureController::showQrResult(const QImage &image)
{
    if (image.isNull())
        return;
    auto *dialog = new QrResultDialog(image, scanBarcodes(image));
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void CaptureController::showOcrResult(const QImage &image)
{
    if (image.isNull())
        return;

    const QString engine = AppSettings::ocrEngine();
    if (engine.isEmpty()) {
        QMessageBox::warning(nullptr, tr("Recognize Text"),
                             tr("No text recognition engine is available in this build."));
        return;
    }

    const QStringList languages = AppSettings::ocrLanguages();
    const QList<ModelFile> missing = Ocr::missingModels(engine, languages);
    if (!missing.isEmpty()) {
        QStringList names;
        for (const ModelFile &file : missing)
            names << file.name;
        const auto answer = QMessageBox::question(
            nullptr, tr("Recognize Text"),
            tr("%1 needs to download its recognition models first:\n\n%2\n\nDownload them now?")
                .arg(Ocr::engineName(engine), names.join(QLatin1Char('\n'))));
        if (answer != QMessageBox::Yes || !ModelStore::download(missing, nullptr))
            return;
    }

    auto *dialog = new OcrResultDialog(image, engine, languages);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
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
