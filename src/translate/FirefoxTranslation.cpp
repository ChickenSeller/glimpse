#include "FirefoxTranslation.h"

#include "glimpse_bergamot.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QLibrary>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <mutex>
#include <vector>

namespace {

struct ModelPart {
    const char *kind; // "model", "lex", "vocab", "srcvocab", "trgvocab"
    const char *file;
    const char *location; // on Mozilla's attachment server
    const char *sha256;
};

struct Direction {
    const char *from;
    const char *to;
    const char *version;
    std::vector<ModelPart> parts;
};

// Firefox's models (Mozilla Remote Settings, collection translations-models),
// pinned per direction: the newest complete desktop release of each.
const std::vector<Direction> &directions()
{
    static const std::vector<Direction> table = {
        {"zh-Hans", "en", "2.0",
            {
                {"model", "model.zhen.intgemm.alphas.bin", "main-workspace/translations-models/052699bf-6f88-4c74-bb14-e49a943b4f59.bin", "3535442962ec8f4a553cc19b206befcac689ee9cddaea44fa91e21527fc30ac2"},
                {"lex", "lex.50.50.zhen.s2t.bin", "main-workspace/translations-models/645c720c-6920-470d-9bb7-3f9a6b0a9cae.bin", "cdcad3592dc2bc4676c34c4d37203f7649ee989195cf083cbb60f1ea011f976b"},
                {"vocab", "vocab.zhen.spm", "main-workspace/translations-models/88a4925d-ff4a-4c76-8813-95e2ac600b14.spm", "dff594318ab7d8b7b60b844ab98ebe6b932ae8045fab15235404c787715965b3"},
            }},
        {"zh-Hant", "en", "2.0",
            {
                {"model", "model.zhen.intgemm.alphas.bin", "main-workspace/translations-models/20260522194230--6ca6db97-6bce-4110-981f-350e15965675--8323efd2-43e9-4886-9f60-35870d1744ea.bin", "0aee91790894458f5d367551f6edcd4c9cb97852c34f221bcbf9f4701ebcf0cd"},
                {"lex", "lex.50.50.zhen.s2t.bin", "main-workspace/translations-models/20260522194215--6c386193-432a-422f-be5f-3cad0977e52b--a52e4a54-5f80-43ce-b873-1bc792ec1483.bin", "aa7daf6cfc85c0cd2c10e2944d66f6da55497c9c6408789f3adfded4074c2fb1"},
                {"srcvocab", "srcvocab.zhen.spm", "main-workspace/translations-models/20260522194213--ad6a9ed2-06ed-4c72-b6ba-aa4492bd582f--72fab356-41da-4bd6-a22c-aa5456ba3ea1.spm", "5cc6a76611dbf86219f109141533606b15ecb34eee83673bb86b2c16b14734db"},
                {"trgvocab", "trgvocab.zhen.spm", "main-workspace/translations-models/20260522194217--5e1d7df1-80a8-4142-ae9d-0ff51b8c2fbc--5e39a274-9805-4d87-a950-39b3d95aac93.spm", "7bf002db37c10d3b114cc5588d7fdcb16c57d0fd1e2c34354c22cc9f0b6c3c29"},
            }},
        {"ja", "en", "2.0",
            {
                {"model", "model.jaen.intgemm.alphas.bin", "main-workspace/translations-models/48d2ba29-c156-44b6-be26-d7ae1192a01b.bin", "a9bf800679bba570520e1161d7b4fbfcb957add32ca35812134add85689752ad"},
                {"lex", "lex.50.50.jaen.s2t.bin", "main-workspace/translations-models/3531750e-147d-444f-b75f-dd077904aaa8.bin", "8f858a72fcbaa476c582577b04d6f5f89d645d2335b0b4a794c2706d4b1f75ff"},
                {"vocab", "vocab.jaen.spm", "main-workspace/translations-models/ca178452-791a-489e-ba83-b90b9bfe6665.spm", "5cb217758bae05877bb3f0c2f612e4e7c1e4cb03c10db11f4a47098d7ae62919"},
            }},
        {"ko", "en", "2.0",
            {
                {"model", "model.koen.intgemm.alphas.bin", "main-workspace/translations-models/df186f1f-866e-4a67-af59-6470c3677938.bin", "1c902d6f7a8d7e3efe6ff4f7d4960a369957bca4ce2ce4a6e8572c231d525090"},
                {"lex", "lex.50.50.koen.s2t.bin", "main-workspace/translations-models/9256bedc-fb5d-4a9c-8a20-476fc246a7cb.bin", "471cd980c4ba08c240246f9361f64eb5d627848a135b5731d665f9efaa1e26ae"},
                {"vocab", "vocab.koen.spm", "main-workspace/translations-models/b82773e2-0043-4160-86de-310155d111fb.spm", "1c72b740ab793cdc3a8f16913dd6b4e806c77421077dd2d85edeb7be38418598"},
            }},
        {"en", "zh-Hans", "2.2",
            {
                {"model", "model.enzh.intgemm.alphas.bin", "main-workspace/translations-models/a7ff7d5e-e67e-406c-a34b-a7edea35b10e.bin", "4e5accc141373565ddc8fa1565bceaa8d0c3482a82cab8131c719ebcc6c2157c"},
                {"lex", "lex.50.50.enzh.s2t.bin", "main-workspace/translations-models/da8fccc0-31df-4665-9703-96d36606e019.bin", "4a5e5827788060f1d718a8132b69440929387514a045796e9b77f935db68c055"},
                {"srcvocab", "srcvocab.enzh.spm", "main-workspace/translations-models/ea98c52c-58dc-45d5-af23-38f2b029d020.spm", "bd9b65504acc6d9726dd281f7defc2adb7c2c22d0688fe2f84697de25197c8c5"},
                {"trgvocab", "trgvocab.enzh.spm", "main-workspace/translations-models/bddbda68-d4d2-4317-a0a1-119caa47525e.spm", "aded6993c36e440284d11cec3f6b8aef9c0e43188a772d80be342a713adf223d"},
            }},
        {"en", "zh-Hant", "2.0",
            {
                {"model", "model.enzh.intgemm.alphas.bin", "main-workspace/translations-models/20260522194155--76d2a34e-7ef2-4829-ac1a-a1319b852dc5--72be7771-db83-4218-bec5-44f021cbf00b.bin", "559ab90d723a58c1f1e2ab7cc12137bc667af5ba3e325e3eb30b5cdc930db520"},
                {"lex", "lex.50.50.enzh.s2t.bin", "main-workspace/translations-models/20260522194159--13a8d548-8d6a-46f6-ab8d-ea19098b7fe8--6cf4bdcb-dbe5-4e2d-af40-a02b440fdcc9.bin", "d891404d1436a7334df12539fe30a26f9e9f2b80bd42fdb8b5f8849e8a1e942b"},
                {"srcvocab", "srcvocab.enzh.spm", "main-workspace/translations-models/20260522194144--b93467f8-2c0b-43f4-a662-19cae5c7f899--ae44a945-6ed0-44d2-88ac-d4b9c25f6c3a.spm", "2266df70492162a249ab1c0154f929bd6098b246544c666c1a0d5a24dde7d2ea"},
                {"trgvocab", "trgvocab.enzh.spm", "main-workspace/translations-models/20260522194157--52f509a0-ac8b-48bf-ba9f-0c61a064a00a--85418cc0-208e-440b-ac6f-c56d242ca430.spm", "22b91a4436d70b91ab8777c677252ab5fae2bc284d71f977df5206c110e3444c"},
            }},
        {"en", "ja", "2.3",
            {
                {"model", "model.enja.intgemm.alphas.bin", "main-workspace/translations-models/4e19636e-4644-4072-8a05-9ef695d4a3ae.bin", "59ae659f9bb63e4f81f474fe3c03d3f4499434b5f9e779fab7c12a45f31fd562"},
                {"lex", "lex.50.50.enja.s2t.bin", "main-workspace/translations-models/87258444-8fac-4d3c-a175-9275e9d10ffa.bin", "edfb7eb47b98a2689b804ab3614c31b24f1257aa54b1154da3d85e1ae8152d9f"},
                {"srcvocab", "srcvocab.enja.spm", "main-workspace/translations-models/cdea5422-fe0d-4941-a653-ca090423e8f8.spm", "970c98d174fc01e0339fbabbf45af36a4be3f26f819ec1a5ea1189f71e091889"},
                {"trgvocab", "trgvocab.enja.spm", "main-workspace/translations-models/5c1a663f-5736-4100-b681-c6737fe79cb5.spm", "3b3d9f8f3a034d98d0a476f1794fa79c01e4e98a967ceb6777a66ba2d03ec1e1"},
            }},
        {"en", "ko", "2.0",
            {
                {"model", "model.enko.intgemm.alphas.bin", "main-workspace/translations-models/4041be52-9e75-41f1-9d4f-04801f112553.bin", "1c310a79b61b8824b2eb26b045db043d92722f3e66ea06998f7c89f48da9f6bc"},
                {"lex", "lex.50.50.enko.s2t.bin", "main-workspace/translations-models/f8a94a6b-264c-4c06-8016-2b57364fbd90.bin", "7734265822ce315c72334ad538e698ab3900ab8e8f04bd43768d0aecad6b6df7"},
                {"vocab", "vocab.enko.spm", "main-workspace/translations-models/2143a895-2f68-4f00-8ebc-40c9563f304d.spm", "709bf0e425345e0e92ce41ed84348a1296a2107c92bdb6624404450ffecbd1f9"},
            }},
        {"en", "fr", "2.0",
            {
                {"model", "model.enfr.intgemm.alphas.bin", "main-workspace/translations-models/e0d7f3ee-163a-4b0a-8e44-8b208b39477d.bin", "6322e296d4fecfe395a8d5723da4ec37ecbe6d7613bb1dfcf4b28e2a47498b68"},
                {"lex", "lex.50.50.enfr.s2t.bin", "main-workspace/translations-models/293d1823-ff05-48c9-837b-cff8fd33f197.bin", "2585ed98d3af0bc949865aedeb390493d591f56870814376e73e4144c41ed059"},
                {"vocab", "vocab.enfr.spm", "main-workspace/translations-models/c7c1bcf3-71f6-425a-a6ed-403bd9a0a759.spm", "783abf3abe075afdf8d85d233994bef2c3a064e935ab1bed946820aff6ac002a"},
            }},
        {"en", "de", "2.1",
            {
                {"model", "model.ende.intgemm.alphas.bin", "main-workspace/translations-models/23db71e7-b6d9-45eb-a47d-0290d7d8ef63.bin", "8df29d9494d19f47fd5d97c6a73474c6f657e9f81c1a607c431d02befdf3810f"},
                {"lex", "lex.50.50.ende.s2t.bin", "main-workspace/translations-models/bc072b1a-7749-43f7-9fe0-34a6dff10c4a.bin", "7ed39f1cffbd68a27ddf05bbfe068de2060f1d7e69f1a20e27ae923551dd7393"},
                {"vocab", "vocab.ende.spm", "main-workspace/translations-models/261225ea-5a52-455b-981c-7d09c6e6da3c.spm", "69f730becafa48e3bb2c244eab66456877c08959a02f2bd5519b5a3088b62f9c"},
            }},
        {"en", "es", "2.1",
            {
                {"model", "model.enes.intgemm.alphas.bin", "main-workspace/translations-models/a4ba0e94-16de-4058-9a44-5bbbbb3c8640.bin", "3b1c399511c01c84c36fae5c0524df44096288efdc8236e182b5c97d7ad2244c"},
                {"lex", "lex.50.50.enes.s2t.bin", "main-workspace/translations-models/1834a61e-0331-4c4a-bbc0-dda02afa8188.bin", "7d51237c0a07027dcd61643cfbbb0f8c48597d19907ef53d2cae9d6bec2cf25c"},
                {"vocab", "vocab.enes.spm", "main-workspace/translations-models/170634fd-511a-4a28-b723-0a1025c67feb.spm", "5ae254fa9b15aa182e70fd2a6186b1333c63a29a48043a9224c6aa4fcac058ad"},
            }},
        {"en", "ru", "2.1a1",
            {
                {"model", "model.enru.intgemm.alphas.bin", "main-workspace/translations-models/1731f496-b7d4-47dc-80d7-8ae222229c9f.bin", "184cb5cda528eeefc0f75f5d0035d787b71d74af135e3c5608d01ae02ecfb920"},
                {"lex", "lex.50.50.enru.s2t.bin", "main-workspace/translations-models/c5d1fcdf-1769-46de-9b31-f1497467b387.bin", "4d91839726b960e70b6d05c53d0cffd16262832b1c0e1ea99d66f412dcc6a239"},
                {"vocab", "vocab.enru.spm", "main-workspace/translations-models/5604e3a8-6087-40d4-bd38-d156c38749ed.spm", "56ee63e14e8cb926c394242adc3ed7cc602644c3d33058cff2ce2959d52a6258"},
            }},
    };
    return table;
}

const Direction *findDirection(const QString &from, const QString &to)
{
    for (const Direction &d : directions()) {
        if (from == QLatin1String(d.from) && to == QLatin1String(d.to))
            return &d;
    }
    return nullptr;
}

QString folderOf(const Direction &d)
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(base).filePath(QStringLiteral("models/firefox/%1-%2").arg(QLatin1String(d.from), QLatin1String(d.to)));
}

QString partPath(const Direction &d, const char *kind)
{
    for (const ModelPart &p : d.parts) {
        if (qstrcmp(p.kind, kind) == 0)
            return QDir(folderOf(d)).filePath(QString::fromLatin1(p.file));
    }
    return {};
}

// English is the hub: X -> en, en -> Y, or X -> en -> Y.
QList<QPair<QString, QString>> route(const QString &from, const QString &to)
{
    const QString en = QStringLiteral("en");
    if (from == to)
        return {};
    if (from == en || to == en)
        return {{from, to}};
    return {{from, en}, {en, to}};
}

QString languageLabel(const QString &code)
{
    if (code == QLatin1String("en"))
        return QStringLiteral("English");
    if (code == QLatin1String("ja"))
        return QStringLiteral("日本語");
    if (code == QLatin1String("ko"))
        return QStringLiteral("한국어");
    if (code.startsWith(QLatin1String("zh")))
        return QStringLiteral("中文");
    return code;
}

// The engine's C interface, resolved from glimpse-bergamot next to the executable.
struct Api {
    QLibrary library;
    decltype(&gb_model_load) modelLoad = nullptr;
    decltype(&gb_model_free) modelFree = nullptr;
    decltype(&gb_translate) translate = nullptr;
    decltype(&gb_string_free) stringFree = nullptr;

    bool load(QString *error)
    {
        if (modelLoad)
            return true;
        library.setFileName(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("glimpse-bergamot")));
        if (!library.load()) {
            *error = QCoreApplication::translate("FirefoxTranslation", "The Firefox translation engine could not be loaded: %1")
                         .arg(library.errorString());
            return false;
        }
        modelLoad = reinterpret_cast<decltype(modelLoad)>(library.resolve("gb_model_load"));
        modelFree = reinterpret_cast<decltype(modelFree)>(library.resolve("gb_model_free"));
        translate = reinterpret_cast<decltype(translate)>(library.resolve("gb_translate"));
        stringFree = reinterpret_cast<decltype(stringFree)>(library.resolve("gb_string_free"));
        if (!modelLoad || !modelFree || !translate || !stringFree) {
            modelLoad = nullptr;
            *error = QCoreApplication::translate("FirefoxTranslation", "The Firefox translation engine is incomplete.");
            return false;
        }
        return true;
    }
};

std::mutex g_mutex; // one translation at a time; guards everything below
Api g_api;
QHash<QString, gb_model *> g_models; // "from-to" -> loaded model

// Checks the files of a direction against their pinned SHA-256 once per run;
// a damaged file is deleted so that the next attempt downloads it again.
bool verifyFiles(const Direction &d, QString *error)
{
    static QSet<QString> verified;
    for (const ModelPart &p : d.parts) {
        const QString path = QDir(folderOf(d)).filePath(QString::fromLatin1(p.file));
        if (verified.contains(path))
            continue;
        QFile file(path);
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file)
            || hash.result().toHex() != QByteArray(p.sha256)) {
            file.close();
            QFile::remove(path);
            *error = QCoreApplication::translate("FirefoxTranslation",
                                                 "A Firefox translation file was damaged and has been removed; "
                                                 "translate again to download it anew.");
            return false;
        }
        verified.insert(path);
    }
    return true;
}

gb_model *modelFor(const QString &from, const QString &to, QString *error)
{
    const QString key = from + QLatin1Char('-') + to;
    if (gb_model *loaded = g_models.value(key))
        return loaded;
    const Direction *d = findDirection(from, to);
    if (!d) {
        *error = QCoreApplication::translate("FirefoxTranslation", "Firefox has no model for %1 to %2.")
                     .arg(languageLabel(from), languageLabel(to));
        return nullptr;
    }
    if (!verifyFiles(*d, error))
        return nullptr;
    const QByteArray model = QFile::encodeName(partPath(*d, "model"));
    const QByteArray lex = QFile::encodeName(partPath(*d, "lex"));
    const QString shared = partPath(*d, "vocab");
    const QByteArray srcVocab = QFile::encodeName(shared.isEmpty() ? partPath(*d, "srcvocab") : shared);
    const QByteArray trgVocab = QFile::encodeName(shared.isEmpty() ? partPath(*d, "trgvocab") : shared);
    char message[1024] = {};
    gb_model *m = g_api.modelLoad(model.constData(), srcVocab.constData(), trgVocab.constData(), lex.constData(),
                                  message, int(sizeof(message)));
    if (!m) {
        *error = QCoreApplication::translate("FirefoxTranslation", "A Firefox translation model could not be loaded: %1")
                     .arg(QString::fromUtf8(message));
        return nullptr;
    }
    g_models.insert(key, m);
    return m;
}

// Sentences from scripts without spaces (Japanese, Chinese) come out joined
// in English: "isn't it?Let's". Put the space back.
QString fixSentenceSpacing(QString text)
{
    static const QRegularExpression joined(QStringLiteral("(?<=[.!?])(?=\\p{Lu})"));
    return text.replace(joined, QStringLiteral(" "));
}

} // namespace

namespace FirefoxTranslation {

QString unavailableReason()
{
    const QString dll = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("glimpse-bergamot.dll"));
    return QFileInfo::exists(dll) ? QString()
                                  : QCoreApplication::translate("FirefoxTranslation",
                                                                "This build does not include the Firefox translation engine.");
}

QString detectLanguage(const QString &text)
{
    int kana = 0, hangul = 0, han = 0, latin = 0;
    for (const QChar c : text) {
        const char16_t u = c.unicode();
        if ((u >= 0x3040 && u <= 0x30ff) || (u >= 0x31f0 && u <= 0x31ff))
            ++kana;
        else if ((u >= 0xac00 && u <= 0xd7af) || (u >= 0x1100 && u <= 0x11ff))
            ++hangul;
        else if (u >= 0x4e00 && u <= 0x9fff)
            ++han;
        else if (c.isLetter() && u < 0x0250)
            ++latin;
    }
    // Japanese mixes kana with kanji; a few kana are enough to tell it apart.
    if (kana > 0 && kana * 10 >= han)
        return QStringLiteral("ja");
    if (hangul > 0 && hangul >= han)
        return QStringLiteral("ko");
    if (han > latin / 4)
        return QStringLiteral("zh-Hans");
    return QStringLiteral("en");
}

QList<ModelFile> missingModels(const QStringList &texts, const QString &target, QString *unsupported)
{
    QList<ModelFile> missing;
    QStringList seen;
    for (const QString &text : texts) {
        const auto steps = route(detectLanguage(text), target);
        for (const auto &[from, to] : steps) {
            const QString key = from + QLatin1Char('-') + to;
            if (seen.contains(key))
                continue;
            seen << key;
            const Direction *d = findDirection(from, to);
            if (!d) {
                *unsupported = QCoreApplication::translate("FirefoxTranslation", "Firefox has no model for %1 to %2.")
                                   .arg(languageLabel(from), languageLabel(to));
                return {};
            }
            for (const ModelPart &p : d->parts) {
                const QString path = QDir(folderOf(*d)).filePath(QString::fromLatin1(p.file));
                if (!QFileInfo::exists(path)) {
                    missing << ModelFile{QStringLiteral("%1 (%2 → %3)").arg(QString::fromLatin1(p.file), from, to),
                                         QUrl(QStringLiteral("https://firefox-settings-attachments.cdn.mozilla.net/")
                                              + QString::fromLatin1(p.location)),
                                         path};
                }
            }
        }
    }
    return missing;
}

QStringList translate(const QStringList &texts, const QString &target, QString *error)
{
    std::lock_guard lock(g_mutex);
    if (!g_api.load(error))
        return {};
    QStringList results(texts.size());
    // Paragraphs of one source language go to the engine together.
    QHash<QString, QList<qsizetype>> bySource;
    for (qsizetype i = 0; i < texts.size(); ++i)
        bySource[detectLanguage(texts[i])] << i;
    for (auto it = bySource.cbegin(); it != bySource.cend(); ++it) {
        const auto steps = route(it.key(), target);
        if (steps.isEmpty()) { // already in the target language
            for (qsizetype i : it.value())
                results[i] = texts[i];
            continue;
        }
        gb_model *first = modelFor(steps[0].first, steps[0].second, error);
        gb_model *second = steps.size() > 1 ? modelFor(steps[1].first, steps[1].second, error) : nullptr;
        if (!first || (steps.size() > 1 && !second))
            return {};

        std::vector<QByteArray> utf8;
        std::vector<const char *> in;
        for (qsizetype i : it.value()) {
            utf8.push_back(texts[i].toUtf8());
            in.push_back(utf8.back().constData());
        }
        std::vector<char *> out(in.size(), nullptr);
        char message[1024] = {};
        if (g_api.translate(first, second, in.data(), int(in.size()), out.data(), message, int(sizeof(message))) != 0) {
            *error = QCoreApplication::translate("FirefoxTranslation", "Firefox translation failed: %1")
                         .arg(QString::fromUtf8(message));
            return {};
        }
        for (size_t k = 0; k < out.size(); ++k) {
            results[it.value()[qsizetype(k)]] = fixSentenceSpacing(QString::fromUtf8(out[k]));
            g_api.stringFree(out[k]);
        }
    }
    return results;
}

void release()
{
    std::unique_lock lock(g_mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;
    for (gb_model *m : std::as_const(g_models))
        g_api.modelFree(m);
    g_models.clear();
}

} // namespace FirefoxTranslation
