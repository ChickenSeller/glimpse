// glimpse-translate-test: runs the local (llama.cpp) translator on a few
// sample paragraphs with the model the settings select, and reports the
// translations and timings.
//
//   glimpse-translate-test [target language name, default 简体中文]
//   glimpse-translate-test --engine google-free [target code, default zh-Hans]

#include "translate/LocalModel.h"
#include "translate/Translator.h"

#include <QCoreApplication>
#include <QElapsedTimer>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    // Same settings and data location as the application.
    QCoreApplication::setOrganizationName(QStringLiteral("Glimpse"));
    QCoreApplication::setApplicationName(QStringLiteral("Glimpse"));

    const QString language = app.arguments().value(1, QStringLiteral("简体中文"));
    const QStringList texts = {
        QStringLiteral("Hold Ctrl+Alt+T and drag over any text on the screen to translate it."),
        QStringLiteral("今日はいい天気ですね。散歩に行きましょう。"),
        QStringLiteral("The quick brown fox jumps over the lazy dog."),
    };

    // --engine <id> <target code>: one of the online engines through Translate::translate.
    if (app.arguments().value(1) == QLatin1String("--engine")) {
        const QString engine = app.arguments().value(2);
        const QString target = app.arguments().value(3, QStringLiteral("zh-Hans"));
        QElapsedTimer timer;
        timer.start();
        int status = 1;
        Translate::translate(engine, texts, target, &app, [&](const QStringList &out, const QString &error) {
            if (!error.isEmpty()) {
                std::printf("FAIL: %s\n", error.toUtf8().constData());
            } else {
                std::printf("%s -> %s: %lld ms\n", engine.toUtf8().constData(), target.toUtf8().constData(),
                            timer.elapsed());
                for (qsizetype i = 0; i < out.size(); ++i)
                    std::printf("  %s\n  -> %s\n", texts[i].toUtf8().constData(), out[i].toUtf8().constData());
                status = 0;
            }
            app.exit(status);
        });
        return app.exec();
    }

    std::printf("model: %s\n", LocalModel::modelPath().toUtf8().constData());
    for (int round = 1; round <= 2; ++round) {
        QElapsedTimer timer;
        timer.start();
        QString error;
        const QStringList out = LocalModel::translate(texts, language, &error);
        if (!error.isEmpty()) {
            std::printf("FAIL: %s\n", error.toUtf8().constData());
            return 1;
        }
        std::printf("round %d (%s): %lld ms\n", round, round == 1 ? "loads the model" : "model loaded", timer.elapsed());
        for (qsizetype i = 0; i < out.size(); ++i)
            std::printf("  %s\n  -> %s\n", texts[i].toUtf8().constData(), out[i].toUtf8().constData());
    }
    LocalModel::release();
    return 0;
}
