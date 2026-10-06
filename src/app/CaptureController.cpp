#include "CaptureController.h"

#include "AboutDialog.h"
#include "AppIcon.h"
#include "AppSettings.h"
#include "CaptureToolbar.h"
#include "DelayCountdown.h"
#include "capture/Platform.h"
#include "color/ColorResultDialog.h"
#include "capture/ScrollCaptureSession.h"
#include "capture/ScreenGrabber.h"
#include "editor/EditorWindow.h"
#include "editor/ImageActions.h"
#include "hotkey/GlobalHotkeys.h"
#include "overlay/RegionSelector.h"
#include "pin/PinWindow.h"
#include "qr/BarcodeScanner.h"
#include "record/RecordingDoneDialog.h"
#include "record/RecordingSession.h"
#include "qr/QrResultDialog.h"
#include "ocr/ModelStore.h"
#include "ocr/OcrResultDialog.h"
#include "settings/SettingsDialog.h"
#include "translate/TranslateResultDialog.h"
#include "translate/FirefoxTranslation.h"
#include "translate/LocalModel.h"
#include "translate/Translator.h"
#include "update/Updater.h"
#include "update/UsageStats.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QTimer>

namespace {

// Time for our own windows (toolbar, tray menu) to disappear before grabbing;
// compositors animate window hiding. Where the toolbar's animation is turned
// off (Platform::hidesWindowsInstantly), it is gone with the next frame
// instead; menus and dialogs still fade out.
constexpr int kHideDelayMs = 300;
constexpr int kShortDelayMs = 150;


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

    // Checked a little after start, once the toolbar is up.
    m_updater = new Updater(this);
    m_updater->setCanRestart([this] { return !m_busy && !m_recording; });
    connect(m_updater, &Updater::notify, this, [this](const QString &title, const QString &message) {
        if (m_tray)
            m_tray->showMessage(title, message);
    });
    QTimer::singleShot(5000, m_updater, &Updater::checkAtStartup);
    QTimer::singleShot(3000, this, [] {
        QString mode = QStringLiteral("Unsupported");
        if (Updater::isSupported()) {
            static const char *const names[] = {"Automatic", "Required only", "Ask", "Never"};
            mode = QString::fromLatin1(names[int(Updater::mode())]);
        }
        UsageStats::reportStart(mode);
    });

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
    m_toolbar->reveal();
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
        m_toolbar->applyLayoutSettings();
        // Register right away to report conflicts while the user can still fix
        // them, then pause again until the dialog closes.
        const QStringList taken = registerHotkeys();
        m_hotkeys->clear();
        m_settings->setHotkeyStatus(m_hotkeyRegistered);
        if (!taken.isEmpty()) {
            QMessageBox::warning(m_settings, tr("Hotkeys unavailable"),
                                 tr("These hotkeys are already in use by another program and will not work:\n%1")
                                     .arg(taken.join(QStringLiteral(", "))));
        }
    });
    connect(m_settings, &QDialog::finished, this, [this] { registerHotkeys(); });
    connect(m_settings, &SettingsDialog::updateNowRequested, m_updater, &Updater::checkNow);
    // As registered before the hotkeys were paused for the dialog.
    m_settings->setHotkeyStatus(m_hotkeyRegistered);
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}

void CaptureController::showAbout()
{
    if (!m_about)
        m_about = new AboutDialog;
    m_about->show();
    m_about->raise();
    m_about->activateWindow();
}

void CaptureController::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_trayMenu = std::make_unique<QMenu>();
    for (CaptureMode mode : kAllCaptureModes) {
        QAction *item = m_trayMenu->addAction(QString(), this, [this, mode] { beginCaptureAfterFade(mode); });
        // The shortcut is only a hint (set in registerHotkeys); the global hotkey does the work.
        item->setShortcutVisibleInContextMenu(true);
        item->setEnabled(captureModeSupported(mode));
        m_trayCaptureActions.insert(mode, item);
    }
    m_trayMenu->addSeparator();
    m_trayShowToolbar = m_trayMenu->addAction(QString(), this, &CaptureController::showToolbar);
    m_traySettings = m_trayMenu->addAction(QString(), this, &CaptureController::showSettings);
    m_trayAbout = m_trayMenu->addAction(QString(), this, &CaptureController::showAbout);
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
        it.value()->setText(tr(captureModeLabel(it.key())));
    if (m_trayShowToolbar) {
        m_trayShowToolbar->setText(tr("Show Toolbar"));
        m_traySettings->setText(tr("Settings..."));
        m_trayAbout->setText(tr("About Glimpse..."));
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
    m_hotkeyRegistered.clear();
    for (CaptureMode mode : kAllCaptureModes) {
        const QKeySequence key = AppSettings::hotkey(mode);
        if (key.isEmpty())
            continue;
        const bool registered = m_hotkeys->add(int(mode), key);
        m_hotkeyRegistered.insert(mode, registered);
        if (registered)
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
    startCapture(mode, false);
}

void CaptureController::beginCaptureAfterFade(Mode mode)
{
    startCapture(mode, true);
}

void CaptureController::startCapture(Mode mode, bool afterFade)
{
    // The record button / hotkey / tray entry also stops a running recording.
    if (mode == Mode::Recording && m_recording) {
        m_recording->stop();
        return;
    }
    if (m_busy || !captureModeSupported(mode))
        return;
    m_busy = true;
    m_mode = mode;

    // Hidden unless the user wants it in the picture.
    m_restoreToolbar = m_toolbar->isVisible() && !AppSettings::captureIncludesToolbar();
    if (m_restoreToolbar)
        m_toolbar->hide();
    // The pointer would sit right on top of the pixel being picked.
    m_grabber->setIncludeCursor(AppSettings::captureIncludesCursor() && Platform::supportsCursorCapture()
                                && mode != Mode::ColorPicker && mode != Mode::Crosshair);

    m_pendingRect.reset();
    m_pendingShape.clear();

    // Full screen has nothing to choose, so its delay comes first. The other
    // modes freeze the screen for choosing the area right away and count down
    // afterwards (see onSnapshot), like FastStone.
    if (mode == Mode::FullScreen && AppSettings::captureDelay() > 0) {
        afterDelay([this] { m_grabber->grab(); });
        return;
    }
    int delay = m_restoreToolbar ? kHideDelayMs : kShortDelayMs;
    if (Platform::hidesWindowsInstantly())
        delay = afterFade ? kShortDelayMs : 0;
    QTimer::singleShot(delay, this, [this] {
        Platform::flushCompositor();
        m_grabber->grab();
    });
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
    else if (m_mode == Mode::Translate)
        showTranslateResult(image);
    else if (m_mode == Mode::Pin) {
        if (AppSettings::copyCapturesToClipboard())
            ImageActions::copyToClipboard(image);
        pinToScreen(image, m_pinRect);
    } else {
        showCapture(image);
    }
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
    const auto selectorMode = m_mode == Mode::Window || m_mode == Mode::Scrolling || m_mode == Mode::Recording
                                  ? RegionSelector::Mode::Window
                              : m_mode == Mode::Freehand                          ? RegionSelector::Mode::Freehand
                              : m_mode == Mode::ColorPicker                       ? RegionSelector::Mode::Color
                              : m_mode == Mode::Crosshair                         ? RegionSelector::Mode::Crosshair
                                                                                  : RegionSelector::Mode::Region;
    auto *selector = new RegionSelector(snapshot, selectorMode, this);
    connect(selector, &RegionSelector::selected, this, [this, selector](const QRect &rect) {
        if (m_mode == Mode::Scrolling) {
            selector->deleteLater();
            startScrollingCapture(rect);
            return;
        }
        if (m_mode == Mode::Recording) {
            // Area first, then the countdown (if any), then recording.
            selector->deleteLater();
            afterDelay([this, rect] { QTimer::singleShot(kShortDelayMs, this, [this, rect] { startRecording(rect); }); });
            return;
        }
        selector->deleteLater();
        const QPainterPath shape = selector->shape();
        m_pinRect = rect;
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
    // Picks from the frozen screen, so the capture delay does not apply.
    connect(selector, &RegionSelector::colorPicked, this, [this, selector](const QColor &color) {
        selector->deleteLater();
        endCapture();
        showColorResult(color);
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
        showCapture(image);
    });
    session->start();
}

void CaptureController::startRecording(const QRect &rect)
{
    // Still "busy" while recording: the toolbar stays hidden (it would be in
    // the video); the record button, hotkey or the panel's Stop end it.
    const QString folder = AppSettings::recordFolder();
    QDir().mkpath(folder);
    ScreenRecorder::Options options;
    options.filePath = QDir(folder).filePath(
        QStringLiteral("Glimpse_%1.mp4").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
    options.frameRate = AppSettings::recordFrameRate();
    options.highlightCursor = AppSettings::recordHighlightCursor() && Platform::supportsPointerHighlight();
    options.showClicks = AppSettings::recordShowClicks() && Platform::supportsInputOverlay();
    options.showKeys = AppSettings::recordShowKeys() && Platform::supportsInputOverlay();
    options.keyStyle = static_cast<ScreenRecorder::KeyStyle>(AppSettings::recordKeyStyle());

    auto *session = new RecordingSession(rect, options, this);
    m_recording = session;
    connect(session, &RecordingSession::finished, this, [this, session](const QString &path, qint64 durationMs) {
        session->deleteLater();
        endCapture();
        auto *dialog = new RecordingDoneDialog(path, durationMs);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    });
    connect(session, &RecordingSession::failed, this, [this, session](const QString &message) {
        session->deleteLater();
        onGrabFailed(tr("Recording failed: %1").arg(message));
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

void CaptureController::showColorResult(const QColor &color)
{
    auto *dialog = new ColorResultDialog(color);
    connect(dialog, &ColorResultDialog::pickAgainRequested, this, [this] { beginCaptureAfterFade(Mode::ColorPicker); });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

bool CaptureController::prepareOcr(const QString &title, QString *engine, QStringList *languages)
{
    *engine = AppSettings::ocrEngine();
    if (engine->isEmpty()) {
        QMessageBox::warning(nullptr, title, tr("No text recognition engine is available in this build."));
        return false;
    }

    *languages = AppSettings::ocrLanguages();
    const QList<ModelFile> missing = Ocr::missingModels(*engine, *languages);
    if (!missing.isEmpty()) {
        QStringList names;
        for (const ModelFile &file : missing)
            names << file.name;
        const auto answer = QMessageBox::question(
            nullptr, title,
            tr("%1 needs to download its recognition models first:\n\n%2\n\nDownload them now?")
                .arg(Ocr::engineName(*engine), names.join(QLatin1Char('\n'))));
        if (answer != QMessageBox::Yes || !ModelStore::download(missing, nullptr))
            return false;
    }
    return true;
}

void CaptureController::showOcrResult(const QImage &image)
{
    QString engine;
    QStringList languages;
    if (image.isNull() || !prepareOcr(tr("Recognize Text"), &engine, &languages))
        return;
    auto *dialog = new OcrResultDialog(image, engine, languages);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void CaptureController::showTranslateResult(const QImage &image)
{
    const QString title = tr("Translate");
    // Without a key every engine fails; offer the settings instead.
    const QString translator = AppSettings::translateEngine();
    if (translator == QLatin1String("firefox")) {
        // Language files are fetched when the text is known (see Translate::translate).
        const QString reason = FirefoxTranslation::unavailableReason();
        if (!reason.isEmpty()) {
            QMessageBox::warning(nullptr, title, reason);
            return;
        }
    } else if (translator == QLatin1String("local")) {
        // The local model: the runtime must be there, and the model downloaded or chosen.
        const QString reason = LocalModel::unavailableReason();
        if (!reason.isEmpty()) {
            QMessageBox::warning(nullptr, title, reason);
            return;
        }
        if (const std::optional<LocalModel::Preset> missing = LocalModel::missingPreset()) {
            const auto answer = QMessageBox::question(
                nullptr, title,
                tr("Local translation needs its model first:\n\n%1\n\nDownload it now? It is stored on this "
                   "computer and used offline from then on.")
                    .arg(missing->name));
            if (answer != QMessageBox::Yes || !ModelStore::download({missing->file}, nullptr))
                return;
        } else if (!QFileInfo::exists(LocalModel::modelPath())) {
            const auto answer = QMessageBox::question(
                nullptr, title, tr("The chosen GGUF model file does not exist. Open Settings > Translation to choose one?"));
            if (answer == QMessageBox::Yes)
                showSettings();
            return;
        }
    } else if (Translate::needsKey(translator) && AppSettings::translateKey(translator).trimmed().isEmpty()) {
        const auto answer = QMessageBox::question(
            nullptr, title,
            tr("%1 needs an API key. Open Settings > Translation to enter one?").arg(Translate::engineName(translator)));
        if (answer == QMessageBox::Yes)
            showSettings();
        return;
    }
    QString engine;
    QStringList languages;
    if (image.isNull() || !prepareOcr(title, &engine, &languages))
        return;
    auto *dialog = new TranslateResultDialog(image, engine, languages);
    connect(dialog, &TranslateResultDialog::translatedImageReady, this, &CaptureController::openEditor);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void CaptureController::pinToScreen(const QImage &image, const QRect &where)
{
    if (image.isNull())
        return;
    // Right where it was captured, so it seems to stay put while the rest moves on.
    auto *pin = new PinWindow(image, where);
    pin->show();
    pin->raise();
    pin->activateWindow();
}

void CaptureController::showCapture(const QImage &image)
{
    if (image.isNull())
        return;
    auto *editor = new EditorWindow(image);
    if (AppSettings::copyCapturesToClipboard())
        editor->copyToClipboard();
    editor->show();
    editor->raise();
    editor->activateWindow();
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
