// glimpse-site-shots: renders the windows the homepage and the guide show
// into PNG files, in one UI language, with the default settings.
//
//   glimpse-site-shots <directory> <en|zh_CN|ja>
//
// Nothing appears on the screen: every window is rendered off screen
// (Qt::WA_DontShowOnScreen) and grabbed, over a made-up desktop. Recognition
// uses PaddleOCR and translation Google's free service, so the models must be
// there (Glimpse downloads them on first use) and the network reachable.
// tools/site-shots.sh runs it for every language into docs/public/images.

#include "app/AppIcon.h"
#include "app/AppSettings.h"
#include "app/CaptureToolbar.h"
#include "app/Language.h"
#include "app/SettingsStore.h"
#include "capture/DesktopSnapshot.h"
#include "color/ColorResultDialog.h"
#include "editor/AnnotatorWindow.h"
#include "editor/EditorWindow.h"
#include "ocr/OcrResultDialog.h"
#include "overlay/RegionSelector.h"
#include "pin/PinWindow.h"
#include "settings/SettingsDialog.h"
#include "whiteboard/WhiteboardCanvas.h"
#include "whiteboard/WhiteboardToolbar.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadialGradient>
#include <QScreen>
#include <QStyleHints>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QThread>
#include <QVBoxLayout>

#include <cstdio>
#include <functional>

namespace {

// The made-up desktop, in logical pixels.
const QSize kDesktop(1600, 900);
// Where the article window sits on it.
const QRect kArticle(420, 170, 760, 420);
// The overlays' desktop lies right of the real screens, like a second
// monitor, so that they follow the events sent to them, not the real pointer.
QPoint g_away;
const int kTitleBar = 32;

QString g_dir;
QString g_language;

void print(const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    std::fwrite(utf8.constData(), 1, size_t(utf8.size()), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

bool waitFor(const std::function<bool()> &done, int ms)
{
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(10);
    }
    return done();
}

void settle(int ms = 400)
{
    waitFor([] { return false; }, ms);
}

void save(const QImage &image, const QString &name)
{
    const QString file = QDir(g_dir).filePath(name);
    if (image.save(file))
        print(QStringLiteral("  ") + file);
    else
        print(QStringLiteral("  FAILED to save ") + file);
}

// Windows are polished before they are shown: from then on they stay off
// the screen, though they lay out, paint and take events as if shown.
class OffScreen : public QObject
{
protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Polish) {
            if (auto *widget = qobject_cast<QWidget *>(watched); widget && widget->isWindow())
                widget->setAttribute(Qt::WA_DontShowOnScreen);
        }
        return false;
    }
};

qreal dpr()
{
    return QGuiApplication::primaryScreen()->devicePixelRatio();
}

QImage blank(const QSize &logical)
{
    QImage image(logical * dpr(), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr());
    image.fill(Qt::transparent);
    return image;
}

// A window as Windows 11 draws it, light: a title bar with the icon, the
// title and the caption buttons, rounded corners and a thin border.
QImage framed(QWidget *window, bool minMax = false)
{
    settle();
    const QPixmap body = window->grab();
    const QSize size(window->width(), window->height() + kTitleBar);
    QImage image = blank(size);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath outline;
    outline.addRoundedRect(QRectF(0.5, 0.5, size.width() - 1, size.height() - 1), 8, 8);
    p.setClipPath(outline);
    p.fillRect(QRect(QPoint(0, 0), size), QColor(0xf3, 0xf3, 0xf3));
    p.drawPixmap(QPoint(0, kTitleBar), body);

    const QIcon icon = window->windowIcon().isNull() ? qApp->windowIcon() : window->windowIcon();
    icon.paint(&p, QRect(12, (kTitleBar - 16) / 2, 16, 16));
    QFont font(QStringLiteral("Segoe UI"));
    font.setPixelSize(12);
    p.setFont(font);
    p.setPen(QColor(0x1a, 0x1a, 0x1a));
    QString title = window->windowTitle();
    title.remove(QStringLiteral("[*]"));
    p.drawText(QRect(38, 0, size.width() - 200, kTitleBar), Qt::AlignVCenter | Qt::AlignLeft, title);

    QFont glyphs(QStringLiteral("Segoe Fluent Icons"));
    glyphs.setPixelSize(10);
    p.setFont(glyphs);
    QStringList buttons = {QStringLiteral("")}; // close
    if (minMax)
        buttons << QStringLiteral("") << QStringLiteral(""); // maximize, minimize
    for (int i = 0; i < buttons.size(); ++i)
        p.drawText(QRect(size.width() - 46 * (i + 1), 0, 46, kTitleBar), Qt::AlignCenter, buttons.at(i));

    p.setClipping(false);
    p.setPen(QPen(QColor(0, 0, 0, 40), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(outline);
    return image;
}

// A soft shadow under a window placed on the desktop.
void drawWindow(QPainter &p, const QImage &window, const QPoint &at)
{
    const QRectF r(at, window.deviceIndependentSize());
    for (int i = 12; i > 0; --i) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 5));
        p.drawRoundedRect(r.adjusted(-i, -i + 8, i, i + 8), 8 + i, 8 + i);
    }
    p.drawImage(at, window);
}

// The page the article window shows: in English, or in Japanese for the
// English guide, so there is something to translate.
QString articleHtml()
{
    if (g_language == QLatin1String("en")) {
        return QStringLiteral(
            "<h1>夜空を見に行く前に</h1>"
            "<p>街の明かりから離れた暗い場所を選び、目が慣れるまで二十分ほど待ちましょう。</p>"
            "<p>双眼鏡があれば、月のクレーターや木星の衛星、星団まで見えます。</p>"
            "<p>暖かい上着を忘れずに。夏の夜でも、深夜を過ぎると冷え込みます。</p>");
    }
    return QStringLiteral(
        "<h1>Before you go out to see the stars</h1>"
        "<p>Find a dark place away from city lights, and give your eyes twenty minutes to adjust.</p>"
        "<p>A pair of binoculars shows more than you might expect: craters on the Moon, "
        "the moons of Jupiter and clusters of stars.</p>"
        "<p>Bring a warm jacket. Even summer nights get cold after midnight.</p>");
}

QImage articleWindow()
{
    QWidget window;
    window.setWindowTitle(g_language == QLatin1String("en") ? QStringLiteral("星空ガイド") : QStringLiteral("Stargazing notes"));
    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *page = new QTextBrowser;
    page->setFrameShape(QFrame::NoFrame);
    page->document()->setDocumentMargin(36);
    page->document()->setDefaultStyleSheet(QStringLiteral(
        "h1 { font-size: 26px; color: #1b2a3a; margin-bottom: 22px; } p { font-size: 17px; color: #2b3540; margin-bottom: 18px; }"));
    QFont font(QStringLiteral("Segoe UI"));
    font.setFamilies({QStringLiteral("Segoe UI"), QStringLiteral("Yu Gothic UI"), QStringLiteral("Microsoft YaHei UI")});
    page->setFont(font);
    page->setHtml(articleHtml());
    page->setStyleSheet(QStringLiteral("QTextBrowser { background: white; }"));
    layout->addWidget(page);
    window.resize(kArticle.width(), kArticle.height() - kTitleBar);
    window.show();
    return framed(&window);
}

// The wallpaper with the article window on it.
QImage desktop()
{
    static QImage cached;
    if (!cached.isNull())
        return cached;
    QImage image = blank(kDesktop);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient sky(0, 0, kDesktop.width(), kDesktop.height());
    sky.setColorAt(0, QColor(0x1d, 0x3b, 0x6e));
    sky.setColorAt(0.55, QColor(0x3d, 0x6f, 0xb6));
    sky.setColorAt(1, QColor(0x8f, 0xb8, 0xe6));
    p.fillRect(QRect(QPoint(0, 0), kDesktop), sky);
    QRadialGradient glow(QPointF(1250, 650), 600);
    glow.setColorAt(0, QColor(255, 255, 255, 90));
    glow.setColorAt(1, QColor(255, 255, 255, 0));
    p.fillRect(QRect(QPoint(0, 0), kDesktop), glow);
    drawWindow(p, articleWindow(), kArticle.topLeft());
    p.end();
    cached = image;
    return cached;
}

// Part of the desktop, in logical pixels, at its full resolution.
QImage crop(const QRect &logical)
{
    const QImage full = desktop();
    QImage part = full.copy(QRect((QPointF(logical.topLeft()) * dpr()).toPoint(), (QSizeF(logical.size()) * dpr()).toSize()));
    part.setDevicePixelRatio(dpr());
    return part;
}

// The article's text, without the window's title bar.
QRect articleText()
{
    return QRect(kArticle.left() + 10, kArticle.top() + kTitleBar + 10, kArticle.width() - 20, 300);
}

void send(QWidget *widget, QEvent::Type type, const QPoint &pos, Qt::MouseButton button = Qt::LeftButton)
{
    const Qt::MouseButtons buttons = type == QEvent::MouseButtonRelease || type == QEvent::MouseMove && button == Qt::NoButton
                                         ? Qt::NoButton
                                         : Qt::MouseButtons(button);
    QMouseEvent event(type, QPointF(pos), QPointF(widget->mapToGlobal(pos)), type == QEvent::MouseMove ? Qt::NoButton : button,
                      buttons, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

void drag(QWidget *widget, const QList<QPoint> &points)
{
    send(widget, QEvent::MouseButtonPress, points.first());
    for (const QPoint &point : points)
        send(widget, QEvent::MouseMove, point);
    send(widget, QEvent::MouseButtonRelease, points.last());
}

template <typename T>
T *topLevel()
{
    const QWidgetList windows = QApplication::topLevelWidgets();
    for (QWidget *window : windows) {
        if (auto *found = qobject_cast<T *>(window))
            return found;
    }
    return nullptr;
}

QWidget *newestWindow(const QWidgetList &before)
{
    const QWidgetList windows = QApplication::topLevelWidgets();
    for (QWidget *window : windows) {
        if (!before.contains(window) && window->isVisible())
            return window;
    }
    return nullptr;
}

DesktopSnapshot snapshot()
{
    DesktopSnapshot snap;
    snap.screens.append({QGuiApplication::primaryScreen()->name(), QRect(g_away, kDesktop), desktop()});
    WindowNode article;
    article.geometry = kArticle.translated(g_away);
    article.title = g_language == QLatin1String("en") ? QStringLiteral("星空ガイド") : QStringLiteral("Stargazing notes");
    article.program = QStringLiteral("notes.exe");
    snap.windows.push_back(article);
    return snap;
}

// The selection overlay over the desktop; `act` moves the mouse on it.
QImage overlay(RegionSelector::Mode mode, const std::function<void(QWidget *)> &act, bool adjustable = false)
{
    const QWidgetList before = QApplication::topLevelWidgets();
    RegionSelector selector(snapshot(), mode);
    selector.setAdjustable(adjustable);
    selector.start();
    QWidget *surface = newestWindow(before);
    if (!surface)
        return {};
    surface->setGeometry(QRect(g_away, kDesktop));
    settle(200);
    act(surface);
    settle();
    return surface->grab().toImage();
}

void shootCapture()
{
    const QRect text = articleText();
    const QImage capture = overlay(RegionSelector::Mode::Region, [&](QWidget *surface) {
        drag(surface, {text.topLeft(), text.center(), text.bottomRight()});
    }, true);
    save(capture, QStringLiteral("capture.png"));
    // The homepage's hero: the same, closer.
    const QRect hero = kArticle.adjusted(-180, -90, 180, 110);
    QImage close = capture.copy(QRect((QPointF(hero.topLeft()) * dpr()).toPoint(), (QSizeF(hero.size()) * dpr()).toSize()));
    close.setDevicePixelRatio(dpr());
    save(close, QStringLiteral("hero.png"));
    save(overlay(RegionSelector::Mode::Window, [&](QWidget *surface) {
             send(surface, QEvent::MouseMove, kArticle.center(), Qt::NoButton);
         }),
         QStringLiteral("capture-window.png"));
    save(overlay(RegionSelector::Mode::Crosshair, [&](QWidget *surface) {
             const QPoint from = text.topLeft() + QPoint(26, 40);
             send(surface, QEvent::MouseMove, from, Qt::NoButton);
             send(surface, QEvent::MouseButtonPress, from);
             send(surface, QEvent::MouseButtonRelease, from);
             send(surface, QEvent::MouseMove, from + QPoint(420, 180), Qt::NoButton);
         }),
         QStringLiteral("crosshair.png"));
}

void shootToolbar()
{
    CaptureToolbar toolbar;
    toolbar.show();
    toolbar.reveal();
    settle(800);
    save(toolbar.grab().toImage(), QStringLiteral("toolbar.png"));
}

// Picks a kImageAnnotator tool by its shortcut key.
void pickTool(QWidget *annotator, Qt::Key key)
{
    const auto actions = annotator->findChildren<QAction *>();
    for (QAction *action : actions) {
        if (action->shortcut() == QKeySequence(key)) {
            action->trigger();
            return;
        }
    }
}

// Draws a few marks with the editor's tools, given in desktop coordinates
// inside `area`, the part of the desktop the editor shows.
void annotate(QWidget *annotator, const QRect &area)
{
    QGraphicsView *view = nullptr;
    const auto views = annotator->findChildren<QGraphicsView *>();
    for (QGraphicsView *candidate : views) {
        if (candidate->isVisible() && candidate->scene())
            view = candidate;
    }
    if (!view)
        return;
    const QRectF scene = view->scene()->itemsBoundingRect();
    const auto at = [&](const QPoint &desktopPos) {
        const QPointF f(qreal(desktopPos.x() - area.left()) / area.width(), qreal(desktopPos.y() - area.top()) / area.height());
        return view->mapFromScene(scene.topLeft() + QPointF(f.x() * scene.width(), f.y() * scene.height()));
    };
    QWidget *viewport = view->viewport();
    const QRect text = articleText();
    pickTool(annotator, Qt::Key_R);
    drag(viewport, {at(text.topLeft() + QPoint(12, 22)), at(text.topLeft() + QPoint(450, 68))});
    pickTool(annotator, Qt::Key_J);
    drag(viewport, {at(text.topLeft() + QPoint(30, 82)), at(text.topLeft() + QPoint(640, 104))});
    pickTool(annotator, Qt::Key_N);
    for (const int y : {93, 145, 197}) {
        send(viewport, QEvent::MouseButtonPress, at(QPoint(kArticle.left() - 24, text.top() + y)));
        send(viewport, QEvent::MouseButtonRelease, at(QPoint(kArticle.left() - 24, text.top() + y)));
    }
    pickTool(annotator, Qt::Key_S);
    settle();
}

void shootEditor()
{
    // Glimpse sets this at start; the preview shows its button only then.
    EditorWindow::setRecognizer([](const QImage &) {});
    {
        EditorWindow editor(crop(articleText()));
        editor.show();
        editor.resize(900, 560);
        save(framed(&editor, true), QStringLiteral("editor.png"));
    }
    {
        // Rectangles as outlines (kImageAnnotator draws them filled at first).
        QSettings settings = SettingsStore::open();
        settings.beginGroup(QStringLiteral("KImageAnnotator"));
        settings.setValue(QStringLiteral("ToolFillType_8"), 0);
        settings.setValue(QStringLiteral("ToolColor_8"), QColor(0xe5, 0x39, 0x35));
        settings.setValue(QStringLiteral("ToolWidth_8"), 3);
        settings.endGroup();
        settings.sync();
        const QRect area = kArticle.adjusted(-60, -50, 60, 50);
        AnnotatorWindow annotator(crop(area));
        annotator.show();
        annotator.resize(1180, 760);
        settle();
        annotate(&annotator, area);
        save(framed(&annotator, true), QStringLiteral("annotate.png"));
    }
}

void shootPin()
{
    const QRect taken(articleText().topLeft() + QPoint(20, 120), QSize(500, 110));
    PinWindow pin(crop(taken));
    pin.show();
    settle();
    const QImage pinned = pin.grab().toImage();
    QImage image = desktop();
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    drawWindow(p, pinned, QPoint(1010, 560));
    p.end();
    save(image, QStringLiteral("pin.png"));
}

void shootWhiteboard()
{
    auto *canvas = new WhiteboardCanvas(QGuiApplication::primaryScreen());
    canvas->setGeometry(QRect(QPoint(0, 0), kDesktop));
    canvas->show();
    settle();
    const QRect text = articleText();
    canvas->setColor(QColor(0xe5, 0x39, 0x35));
    canvas->setTool(WhiteboardCanvas::Tool::Ellipse);
    drag(canvas, {text.topLeft() + QPoint(10, 10), text.topLeft() + QPoint(560, 70)});
    canvas->setTool(WhiteboardCanvas::Tool::Arrow);
    drag(canvas, {QPoint(1250, 300), QPoint(1100, 360), QPoint(text.left() + 600, 200)});
    canvas->setTool(WhiteboardCanvas::Tool::Highlighter);
    canvas->setColor(QColor(0xff, 0xd6, 0x00));
    drag(canvas, {text.topLeft() + QPoint(30, 175), text.topLeft() + QPoint(400, 175), text.topLeft() + QPoint(690, 175)});
    canvas->setTool(WhiteboardCanvas::Tool::Pen);
    settle();
    const QImage ink = canvas->grab().toImage();
    WhiteboardToolbar *toolbar = topLevel<WhiteboardToolbar>();
    QImage image = desktop();
    QPainter p(&image);
    p.drawImage(QPoint(0, 0), ink);
    if (toolbar) {
        const QImage tools = toolbar->grab().toImage();
        p.drawImage(QPoint((kDesktop.width() - toolbar->width()) / 2, 12), tools);
    }
    p.end();
    save(image, QStringLiteral("whiteboard.png"));
    canvas->close();
}

void shootOcr()
{
    const QString target = g_language == QLatin1String("zh_CN") ? QStringLiteral("zh-Hans") : g_language == QLatin1String("ja") ? QStringLiteral("ja") : QStringLiteral("en");
    AppSettings::setTranslateTarget(target);
    AppSettings::setTranslateEngine(QStringLiteral("google-free"));
    OcrResultDialog dialog(crop(articleText()), QStringLiteral("paddle"), AppSettings::ocrLanguages());
    QImage translated;
    QObject::connect(&dialog, &OcrResultDialog::translateRequested, &dialog, [&dialog] { dialog.translate(); });
    QObject::connect(&dialog, &OcrResultDialog::translatedImageReady, &dialog, [&translated](const QImage &image) { translated = image; });
    dialog.show();
    dialog.resize(1000, 780);
    auto *source = dialog.findChild<QPlainTextEdit *>(QStringLiteral("sourceEdit"));
    auto *result = dialog.findChild<QPlainTextEdit *>(QStringLiteral("targetEdit"));
    if (!waitFor([source] { return !source->toPlainText().isEmpty(); }, 120000)) {
        print(QStringLiteral("  FAILED: nothing recognized"));
        return;
    }
    dialog.translate();
    if (!waitFor([result] { return !result->toPlainText().isEmpty(); }, 60000)) {
        print(QStringLiteral("  FAILED: not translated"));
        return;
    }
    settle(600);
    save(framed(&dialog, true), QStringLiteral("ocr.png"));
    if (auto *button = dialog.findChild<QPushButton *>(QStringLiteral("imageButton")))
        button->click();
    if (waitFor([&translated] { return !translated.isNull(); }, 30000)) {
        save(translated, QStringLiteral("translated.png"));
        save(crop(articleText()), QStringLiteral("original.png"));
    }
}

void shootSettings()
{
    SettingsDialog dialog;
    dialog.show();
    auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("tabs"));
    const QStringList names = {QStringLiteral("general"), QStringLiteral("hotkeys"), QStringLiteral("toolbar"),
                               QStringLiteral("recording"), QStringLiteral("ocr"), QStringLiteral("translation")};
    for (int i = 0; i < tabs->count() && i < names.size(); ++i) {
        tabs->setCurrentIndex(i);
        settle(200);
        save(framed(&dialog), QStringLiteral("settings-%1.png").arg(names.at(i)));
    }
}

void shootColor()
{
    ColorResultDialog dialog(QColor(0x3d, 0x6f, 0xb6));
    dialog.show();
    save(framed(&dialog), QStringLiteral("color.png"));
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Glimpse"));
    QApplication::setOrganizationName(QStringLiteral("Glimpse"));
    QApplication::setApplicationVersion(QStringLiteral(GLIMPSE_VERSION));
    const QStringList args = QApplication::arguments();
    if (args.size() < 3) {
        print(QStringLiteral("usage: glimpse-site-shots <directory> <en|zh_CN|ja> [shot...]"));
        return 2;
    }
    g_dir = args.at(1);
    g_language = args.at(2);
    const QStringList only = args.mid(3);
    QDir().mkpath(g_dir);

    // The default settings, whatever this machine has.
    QTemporaryDir settings;
    SettingsStore::useFile(settings.filePath(QStringLiteral("config.ini")));
    AppSettings::setLanguage(g_language);
    // Not the build folder the default would show.
    AppSettings::setSaveFolder(QStringLiteral("C:/Tools/Glimpse/output"));
    Language::apply(g_language);
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
    QApplication::setWindowIcon(appIcon());
    const QRect screens = QGuiApplication::primaryScreen()->virtualGeometry();
    g_away = QPoint(screens.right() + 1, 0);
    OffScreen offScreen;
    app.installEventFilter(&offScreen);

    const QList<std::pair<QString, std::function<void()>>> shots = {
        {QStringLiteral("capture"), shootCapture},     {QStringLiteral("toolbar"), shootToolbar},
        {QStringLiteral("editor"), shootEditor},       {QStringLiteral("pin"), shootPin},
        {QStringLiteral("whiteboard"), shootWhiteboard},
        {QStringLiteral("ocr"), shootOcr},             {QStringLiteral("settings"), shootSettings},
        {QStringLiteral("color"), shootColor},
    };
    for (const auto &[name, shoot] : shots) {
        if (!only.isEmpty() && !only.contains(name))
            continue;
        print(name);
        shoot();
    }
    return 0;
}
