// glimpse-record-test: records a few seconds of an area of the primary screen
// to an MP4 with every overlay on, then opens the file again and reports its
// duration and frame size and saves one decoded frame as a PNG.
//
//   glimpse-record-test [seconds] [--rect x,y,w,h] [--out file.mp4] [--keys n]
//       [--mic] [--system] [--pause] [--sync]
//       (n: ScreenRecorder::KeyPosition as a number, 9 for below the picture;
//       --mic and --system record sound, --pause pauses for a second halfway;
//       --sync covers the area with a window that flashes white once a second
//       as a beep plays, to compare the picture and the sound in the file)
//   glimpse-record-test --probe file.mp4 [ms]   (only reads a file back)
//   glimpse-record-test --keyboard-preview out.png [layout...]   (draws the key
//       overlay under the layout in use, or under each named one: 0000040C)

#include "record/InputMonitor.h"
#include "record/KeyboardOverlay.h"
#include "record/ScreenRecorder.h"

#include <QApplication>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>
#include <QWidget>

#include <cmath>
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
            std::printf("decoded frame %dx%d at %lld ms, duration %lld ms, %s -> %s\n", image.width(), image.height(),
                        frame.startTime() / 1000, player->duration(),
                        player->hasAudio() ? "with sound" : "no sound", qPrintable(png));
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
        QList<KeyboardLayout> layouts;
        for (const QString &name : args.mid(3))
            layouts << keyboardLayoutNamed(name);
        if (layouts.isEmpty())
            layouts << currentKeyboardLayout();
        const qreal unit = 34.0 * 1.5;
        const QSize size = KeyboardOverlay::sizeFor(unit);
        QImage image(size.width() + 48, int(layouts.size()) * (size.height() + 24) + 24, QImage::Format_ARGB32);
        image.fill(QColor(0xf0, 0xf0, 0xf0));
        QPainter painter(&image);
        for (int i = 0; i < layouts.size(); ++i) {
            KeyboardOverlay keyboard;
            keyboard.setLayout(layouts.at(i));
            for (int key : {0x1D, 0x2A, 0x1F}) // left Ctrl, left Shift, the key of S on US keyboards
                keyboard.setKey(key, true, 0);
            keyboard.setKey(0x39, true, 0);
            keyboard.setKey(0x39, false, 1000); // Space, released 150 ms ago
            keyboard.paint(painter, QRectF(24, 24 + i * (size.height() + 24), size.width(), size.height()), 1150);
        }
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
    int keyPosition = 0;
    bool microphone = false;
    bool systemAudio = false;
    bool pause = false;
    bool sync = false;
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == QLatin1String("--rect") && i + 1 < args.size()) {
            const QStringList v = args[++i].split(QLatin1Char(','));
            if (v.size() == 4)
                rect = QRect(v[0].toInt(), v[1].toInt(), v[2].toInt(), v[3].toInt());
        } else if (args[i] == QLatin1String("--out") && i + 1 < args.size()) {
            out = args[++i];
        } else if (args[i] == QLatin1String("--keys") && i + 1 < args.size()) {
            keyPosition = args[++i].toInt();
        } else if (args[i] == QLatin1String("--mic")) {
            microphone = true;
        } else if (args[i] == QLatin1String("--system")) {
            systemAudio = true;
        } else if (args[i] == QLatin1String("--pause")) {
            pause = true;
        } else if (args[i] == QLatin1String("--sync")) {
            sync = true;
        } else {
            seconds = std::max(1, args[i].toInt());
        }
    }
    QFile::remove(out);

    ScreenRecorder::Options options;
    options.filePath = out;
    options.showKeys = true;
    options.keyPosition = static_cast<ScreenRecorder::KeyPosition>(keyPosition);
    options.microphone = microphone;
    options.systemAudio = systemAudio;
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
    if (sync) {
        auto *flash = new QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        flash->setGeometry(rect);
        flash->setAutoFillBackground(true);
        const auto paint = [flash](const QColor &color) {
            QPalette palette = flash->palette();
            palette.setColor(QPalette::Window, color);
            flash->setPalette(palette);
            flash->repaint();
        };
        paint(Qt::black);
        flash->show();
        // A 1 kHz beep of 100 ms, pushed to the default output as the
        // window turns white.
        QAudioFormat format;
        format.setSampleRate(48000);
        format.setChannelCount(1);
        format.setSampleFormat(QAudioFormat::Float);
        auto *sink = new QAudioSink(QMediaDevices::defaultAudioOutput(), format);
        QIODevice *speaker = sink->start();
        QByteArray beep(4800 * sizeof(float), Qt::Uninitialized);
        for (int i = 0; i < 4800; ++i)
            reinterpret_cast<float *>(beep.data())[i] = 0.5f * float(std::sin(2 * M_PI * 1000 * i / 48000.0));
        auto *timer = new QTimer;
        QObject::connect(timer, &QTimer::timeout, [=] {
            paint(Qt::white);
            // What is queued before the beep plays after it.
            std::printf("beep after %lld ms queued\n",
                        qint64(format.durationForBytes(sink->bufferSize() - sink->bytesFree()) / 1000));
            speaker->write(beep);
            QTimer::singleShot(100, [=] { paint(Qt::black); });
        });
        timer->start(1000);
    }
    recorder->start();
    if (pause) {
        QTimer::singleShot(seconds * 500, recorder, [recorder] { recorder->setPaused(true); });
        QTimer::singleShot(seconds * 500 + 1000, recorder, [recorder] { recorder->setPaused(false); });
    }
    QTimer::singleShot(seconds * 1000 + (pause ? 1000 : 0), recorder, &ScreenRecorder::stop);
    QTimer::singleShot((seconds + 30) * 1000, [] {
        std::printf("FAIL: timeout\n");
        QCoreApplication::exit(1);
    });
    return app.exec();
}
