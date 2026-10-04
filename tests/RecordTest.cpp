// glimpse-record-test: records a few seconds of an area of the primary screen
// to an MP4 with every overlay on, then opens the file again and reports its
// duration and frame size and saves one decoded frame as a PNG.
//
//   glimpse-record-test [seconds] [--rect x,y,w,h] [--out file.mp4]
//   glimpse-record-test --probe file.mp4 [ms]   (only reads a file back)
//   glimpse-record-test --keyboard-preview out.png   (draws the key overlay)

#include "record/KeyboardOverlay.h"
#include "record/ScreenRecorder.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMediaPlayer>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

#include <cstdio>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const QStringList args = app.arguments();

    // Reads a video back: duration, frame size, and the frame at `ms` as a PNG.
    const auto probe = [](const QString &path, qint64 ms) {
        auto *player = new QMediaPlayer;
        auto *sink = new QVideoSink;
        player->setVideoSink(sink);
        auto saved = std::make_shared<bool>(false);
        QObject::connect(sink, &QVideoSink::videoFrameChanged, [=](const QVideoFrame &frame) {
            if (*saved || !frame.isValid() || frame.startTime() < ms * 1000)
                return;
            *saved = true;
            const QImage image = frame.toImage();
            const QString png = QDir::temp().filePath(QStringLiteral("glimpse-record-test.png"));
            image.save(png);
            std::printf("decoded frame %dx%d at %lld ms, duration %lld ms -> %s\n", image.width(), image.height(),
                        frame.startTime() / 1000, player->duration(), qPrintable(png));
            QCoreApplication::exit(0);
        });
        QObject::connect(player, &QMediaPlayer::errorOccurred, [](QMediaPlayer::Error, const QString &message) {
            std::printf("FAIL playing back: %s\n", qPrintable(message));
            QCoreApplication::exit(1);
        });
        player->setSource(QUrl::fromLocalFile(path));
        player->play();
        QTimer::singleShot(15000, [] {
            std::printf("FAIL: no frame decoded\n");
            QCoreApplication::exit(1);
        });
    };
    // Draws the on-video keyboard with Ctrl+Shift+S held and Space just
    // released over a sample background, without touching real input.
    if (args.size() >= 3 && args[1] == QLatin1String("--keyboard-preview")) {
        QImage image(1200, 800, QImage::Format_ARGB32);
        image.fill(QColor(0xf0, 0xf0, 0xf0));
        KeyboardOverlay keyboard;
        for (int vk : {0xA2, 0xA0, int('S')}) // left Ctrl, left Shift, S
            keyboard.setKey(vk, true, 0);
        keyboard.setKey(0x20, true, 0);
        keyboard.setKey(0x20, false, 1000); // Space, released 150 ms ago
        QPainter painter(&image);
        const qreal unit = std::min({image.width() * 0.6 / 19.1, image.height() / 3.0 / 7.1, 34.0 * 1.5});
        const QSize size = KeyboardOverlay::sizeFor(unit);
        keyboard.paint(painter, QRectF((image.width() - size.width()) / 2.0, image.height() - 24 - size.height(),
                                       size.width(), size.height()),
                       1150);
        painter.end();
        image.save(args[2]);
        std::printf("keyboard %dx%d -> %s\n", size.width(), size.height(), qPrintable(args[2]));
        return 0;
    }
    if (args.size() >= 3 && args[1] == QLatin1String("--probe")) {
        probe(args[2], args.size() > 3 ? args[3].toLongLong() : 1000);
        return app.exec();
    }

    int seconds = 3;
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect rect = QRect(QPoint(0, 0), QSize(800, 600));
    rect.moveCenter(screen->geometry().center());
    QString out = QDir::temp().filePath(QStringLiteral("glimpse-record-test.mp4"));
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == QLatin1String("--rect") && i + 1 < args.size()) {
            const QStringList v = args[++i].split(QLatin1Char(','));
            if (v.size() == 4)
                rect = QRect(v[0].toInt(), v[1].toInt(), v[2].toInt(), v[3].toInt());
        } else if (args[i] == QLatin1String("--out") && i + 1 < args.size()) {
            out = args[++i];
        } else {
            seconds = std::max(1, args[i].toInt());
        }
    }
    QFile::remove(out);

    ScreenRecorder::Options options;
    options.filePath = out;
    options.showKeys = true;
    auto *recorder = new ScreenRecorder(screen, rect, options);

    QObject::connect(recorder, &ScreenRecorder::failed, [](const QString &message) {
        std::printf("FAIL: %s\n", qPrintable(message));
        QCoreApplication::exit(1);
    });
    QObject::connect(recorder, &ScreenRecorder::finished, [&](const QString &path) {
        std::printf("written %d frames, dropped %d, %.1f s; file %s (%lld bytes)\n", recorder->framesWritten(),
                    recorder->framesDropped(), recorder->durationMs() / 1000.0, qPrintable(path),
                    QFileInfo(path).size());

        probe(path, 1000);
    });

    std::printf("recording %d s of %d,%d %dx%d (logical) on %s\n", seconds, rect.x(), rect.y(), rect.width(),
                rect.height(), qPrintable(screen->name()));
    recorder->start();
    QTimer::singleShot(seconds * 1000, recorder, &ScreenRecorder::stop);
    QTimer::singleShot((seconds + 30) * 1000, [] {
        std::printf("FAIL: timeout\n");
        QCoreApplication::exit(1);
    });
    return app.exec();
}
