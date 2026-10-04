#include "Translator.h"

#include "FirefoxTranslation.h"
#include "LocalModel.h"
#include "ocr/ModelStore.h"
#include "app/AppSettings.h"

#include <QCoreApplication>
#include <QFutureWatcher>
#include <QMessageBox>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrlQuery>

namespace {

QNetworkAccessManager *network()
{
    static auto *manager = new QNetworkAccessManager(QCoreApplication::instance());
    return manager;
}

// The engines' own codes for our target languages.
QString deeplCode(const QString &code)
{
    static const QHash<QString, QString> codes = {
        {QStringLiteral("zh-Hans"), QStringLiteral("ZH-HANS")}, {QStringLiteral("zh-Hant"), QStringLiteral("ZH-HANT")},
        {QStringLiteral("en"), QStringLiteral("EN-US")}};
    return codes.value(code, code.toUpper());
}

QString googleCode(const QString &code)
{
    static const QHash<QString, QString> codes = {{QStringLiteral("zh-Hans"), QStringLiteral("zh-CN")},
                                                  {QStringLiteral("zh-Hant"), QStringLiteral("zh-TW")}};
    return codes.value(code, code);
}

// The best readable message from a failed reply: the service's own message
// when its error body has one, else the HTTP status or network error.
QString replyError(QNetworkReply *reply, const QByteArray &body)
{
    const QJsonObject json = QJsonDocument::fromJson(body).object();
    QString message;
    const QJsonValue error = json.value(QStringLiteral("error"));
    if (error.isObject())
        message = error.toObject().value(QStringLiteral("message")).toString(); // Google, Microsoft, Claude
    else if (json.contains(QStringLiteral("message")))
        message = json.value(QStringLiteral("message")).toString(); // DeepL
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 401 || status == 403)
        return QCoreApplication::translate("Translate", "The API key was rejected (HTTP %1). Check it in Settings > Translation.").arg(status)
               + (message.isEmpty() ? QString() : QStringLiteral("\n") + message);
    if (!message.isEmpty())
        return status ? QStringLiteral("HTTP %1: %2").arg(status).arg(message) : message;
    return reply->errorString();
}

using Parser = std::function<QStringList(const QByteArray &body, QString *error)>;

void send(QNetworkRequest request, const QByteArray &body, int expected, QObject *context,
          const Translate::Callback &done, const Parser &parse)
{
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(60000);
    QNetworkReply *reply = network()->post(request, body);
    QPointer<QObject> guard(context);
    QObject::connect(reply, &QNetworkReply::finished, context, [reply, guard, done, parse, expected] {
        reply->deleteLater();
        if (!guard)
            return;
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            done({}, replyError(reply, data));
            return;
        }
        QString error;
        const QStringList result = parse(data, &error);
        if (error.isEmpty() && result.size() != expected)
            error = QCoreApplication::translate("Translate", "The service returned %1 translations for %2 paragraphs.").arg(result.size()).arg(expected);
        done(error.isEmpty() ? result : QStringList(), error);
    });
}

void deepl(const QStringList &texts, const QString &target, const QString &key, QObject *context,
           const Translate::Callback &done)
{
    // Keys of the free plan end in ":fx" and use their own host.
    const QString host = key.endsWith(QLatin1String(":fx")) ? QStringLiteral("api-free.deepl.com")
                                                            : QStringLiteral("api.deepl.com");
    QNetworkRequest request(QUrl(QStringLiteral("https://%1/v2/translate").arg(host)));
    request.setRawHeader("Authorization", "DeepL-Auth-Key " + key.toUtf8());
    const QJsonObject body{{QStringLiteral("text"), QJsonArray::fromStringList(texts)},
                           {QStringLiteral("target_lang"), deeplCode(target)}};
    send(request, QJsonDocument(body).toJson(QJsonDocument::Compact), texts.size(), context, done,
         [](const QByteArray &data, QString *) {
             QStringList out;
             const QJsonArray list = QJsonDocument::fromJson(data).object().value(QStringLiteral("translations")).toArray();
             for (const QJsonValue &item : list)
                 out << item.toObject().value(QStringLiteral("text")).toString();
             return out;
         });
}

// The free service Google Translate's Chrome extension uses: no key, one
// request for all paragraphs. Unofficial, so it may change or be limited.
void googleFree(const QStringList &texts, const QString &target, QObject *context, const Translate::Callback &done)
{
    QUrl url(QStringLiteral("https://clients5.google.com/translate_a/t"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client"), QStringLiteral("dict-chrome-ex"));
    query.addQueryItem(QStringLiteral("sl"), QStringLiteral("auto"));
    query.addQueryItem(QStringLiteral("tl"), googleCode(target));
    url.setQuery(query);
    QNetworkRequest request(url);
    // It answers browsers; a bare client gets an error page.
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                                     "(KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36"));
    // One q= per paragraph, UTF-8, form-encoded; the answer keeps their order.
    QByteArray form;
    for (const QString &text : texts)
        form += (form.isEmpty() ? "q=" : "&q=") + QUrl::toPercentEncoding(text);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    request.setTransferTimeout(30000);
    QNetworkReply *reply = network()->post(request, form);
    QPointer<QObject> guard(context);
    const qsizetype expected = texts.size();
    QObject::connect(reply, &QNetworkReply::finished, context, [reply, guard, done, expected] {
        reply->deleteLater();
        if (!guard)
            return;
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            done({}, QCoreApplication::translate("Translate", "Google's free service did not answer (%1). It may be "
                                                              "limiting requests from this network; try another service.")
                         .arg(reply->errorString()));
            return;
        }
        // One entry per text: [translation, detected language] with automatic
        // detection, or a plain string.
        QStringList out;
        const QJsonArray list = QJsonDocument::fromJson(data).array();
        for (const QJsonValue &item : list)
            out << (item.isArray() ? item.toArray().first().toString() : item.toString());
        if (out.size() != expected) {
            done({}, QCoreApplication::translate("Translate", "Google's free service sent an unexpected answer. It may "
                                                              "have changed; try another service."));
            return;
        }
        done(out, {});
    });
}

void google(const QStringList &texts, const QString &target, const QString &key, QObject *context,
            const Translate::Callback &done)
{
    QUrl url(QStringLiteral("https://translation.googleapis.com/language/translate/v2"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("key"), key);
    url.setQuery(query);
    const QJsonObject body{{QStringLiteral("q"), QJsonArray::fromStringList(texts)},
                           {QStringLiteral("target"), googleCode(target)},
                           {QStringLiteral("format"), QStringLiteral("text")}};
    send(QNetworkRequest(url), QJsonDocument(body).toJson(QJsonDocument::Compact), texts.size(), context, done,
         [](const QByteArray &data, QString *) {
             QStringList out;
             const QJsonArray list = QJsonDocument::fromJson(data)
                                         .object()
                                         .value(QStringLiteral("data"))
                                         .toObject()
                                         .value(QStringLiteral("translations"))
                                         .toArray();
             for (const QJsonValue &item : list)
                 out << item.toObject().value(QStringLiteral("translatedText")).toString();
             return out;
         });
}

void microsoft(const QStringList &texts, const QString &target, const QString &key, QObject *context,
               const Translate::Callback &done)
{
    QUrl url(QStringLiteral("https://api.cognitive.microsofttranslator.com/translate"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("api-version"), QStringLiteral("3.0"));
    query.addQueryItem(QStringLiteral("to"), target);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setRawHeader("Ocp-Apim-Subscription-Key", key.toUtf8());
    // Regional (non-global) resources need their region named.
    const QString region = AppSettings::microsoftTranslatorRegion();
    if (!region.isEmpty())
        request.setRawHeader("Ocp-Apim-Subscription-Region", region.toUtf8());
    QJsonArray body;
    for (const QString &text : texts)
        body.append(QJsonObject{{QStringLiteral("Text"), text}});
    send(request, QJsonDocument(body).toJson(QJsonDocument::Compact), texts.size(), context, done,
         [](const QByteArray &data, QString *) {
             QStringList out;
             const QJsonArray list = QJsonDocument::fromJson(data).array();
             for (const QJsonValue &item : list) {
                 const QJsonArray translations = item.toObject().value(QStringLiteral("translations")).toArray();
                 out << translations.first().toObject().value(QStringLiteral("text")).toString();
             }
             return out;
         });
}

void claude(const QStringList &texts, const QString &target, const QString &key, QObject *context,
            const Translate::Callback &done)
{
    QNetworkRequest request(QUrl(QStringLiteral("https://api.anthropic.com/v1/messages")));
    request.setRawHeader("x-api-key", key.toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");
    // A declined request is re-run on Anthropic's recommended fallback model.
    request.setRawHeader("anthropic-beta", "server-side-fallback-2026-07-01");

    const QString system = QStringLiteral(
        "You translate text recognized from a screenshot (it may contain OCR mistakes; read through them). "
        "The user message is a JSON array of paragraphs. Translate each paragraph into %1, keeping its meaning, "
        "tone, numbers, names and line breaks. Reply with only a JSON array of the translated strings, the same "
        "length and order as the input, and nothing else.")
                               .arg(Translate::languageName(target));
    const QJsonObject body{
        {QStringLiteral("model"), QStringLiteral("claude-opus-5-5")},
        {QStringLiteral("max_tokens"), 16000},
        {QStringLiteral("fallbacks"), QStringLiteral("default")},
        // Translation needs little reasoning; low effort keeps it quick.
        {QStringLiteral("output_config"), QJsonObject{{QStringLiteral("effort"), QStringLiteral("low")}}},
        {QStringLiteral("system"), system},
        {QStringLiteral("messages"),
         QJsonArray{QJsonObject{
             {QStringLiteral("role"), QStringLiteral("user")},
             {QStringLiteral("content"),
              QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(texts)).toJson(QJsonDocument::Compact))}}}}};
    send(request, QJsonDocument(body).toJson(QJsonDocument::Compact), texts.size(), context, done,
         [](const QByteArray &data, QString *error) {
             const QJsonObject json = QJsonDocument::fromJson(data).object();
             // A refusal is a successful reply without the answer.
             if (json.value(QStringLiteral("stop_reason")).toString() == QLatin1String("refusal")) {
                 *error = QCoreApplication::translate("Translate", "Claude declined to translate this text.");
                 return QStringList();
             }
             QString text;
             const QJsonArray content = json.value(QStringLiteral("content")).toArray();
             for (const QJsonValue &block : content) {
                 if (block.toObject().value(QStringLiteral("type")).toString() == QLatin1String("text"))
                     text += block.toObject().value(QStringLiteral("text")).toString();
             }
             // The array alone, even if a code fence or a remark slipped in.
             const qsizetype start = text.indexOf(QLatin1Char('['));
             const qsizetype end = text.lastIndexOf(QLatin1Char(']'));
             const QJsonArray array = start >= 0 && end > start
                                          ? QJsonDocument::fromJson(text.mid(start, end - start + 1).toUtf8()).array()
                                          : QJsonArray();
             QStringList out;
             for (const QJsonValue &item : array)
                 out << item.toString();
             if (out.isEmpty())
                 *error = QCoreApplication::translate("Translate", "Claude's reply could not be read.");
             return out;
         });
}

} // namespace

namespace Translate {

QStringList engineIds()
{
    return {QStringLiteral("google-free"), QStringLiteral("firefox"),   QStringLiteral("local"),
            QStringLiteral("deepl"),       QStringLiteral("google"),    QStringLiteral("microsoft"),
            QStringLiteral("claude")};
}

QString engineName(const QString &id)
{
    if (id == QLatin1String("google-free"))
        return QCoreApplication::translate("Translate", "Google Translate (free, online)");
    if (id == QLatin1String("firefox"))
        return QCoreApplication::translate("Translate", "Firefox translation (offline, light)");
    if (id == QLatin1String("local"))
        return QCoreApplication::translate("Translate", "Local model (llama.cpp, offline)");
    if (id == QLatin1String("deepl"))
        return QStringLiteral("DeepL");
    if (id == QLatin1String("google"))
        return QCoreApplication::translate("Translate", "Google Cloud Translation");
    if (id == QLatin1String("microsoft"))
        return QCoreApplication::translate("Translate", "Microsoft Translator");
    if (id == QLatin1String("claude"))
        return QStringLiteral("Claude (Anthropic)");
    return id;
}

QString engineDescription(const QString &id)
{
    if (id == QLatin1String("google-free"))
        return QCoreApplication::translate("Translate", "The free service behind Google Translate in Chrome: no key, good quality, "
                                                        "many languages. It is unofficial, so Google may limit or change it; "
                                                        "if it stops working, switch to another service.");
    if (id == QLatin1String("firefox"))
        return QCoreApplication::translate("Translate", "The engine behind Firefox's offline translations: small models "
                                                        "(35-70 MB per language direction, downloaded on first use) that "
                                                        "run fast on any computer. Translates between English and other "
                                                        "languages; others, such as Chinese and Japanese, go through English.");
    if (id == QLatin1String("local"))
        return QCoreApplication::translate("Translate", "Runs a language model on this computer: nothing leaves it and no key is needed. "
                                                        "The model is downloaded on first use; a graphics card makes it much faster.");
    if (id == QLatin1String("deepl"))
        return QCoreApplication::translate("Translate", "Natural translations, especially between European languages and Japanese. "
                  "The free plan includes 500,000 characters a month; get a key at deepl.com.");
    if (id == QLatin1String("google"))
        return QCoreApplication::translate("Translate", "Many languages and fast. Needs a Google Cloud API key with the Cloud Translation API enabled.");
    if (id == QLatin1String("microsoft"))
        return QCoreApplication::translate("Translate", "Many languages, with a free tier of 2 million characters a month. Needs an Azure Translator "
                  "key, and its region unless the resource is global.");
    if (id == QLatin1String("claude"))
        return QCoreApplication::translate("Translate", "A large language model: reads through OCR mistakes and keeps the tone and terms in context. "
                  "Needs an Anthropic API key; billed per use.");
    return {};
}

QString defaultEngine()
{
    // Works out of the box: no key, nothing to download.
    return QStringLiteral("google-free");
}

QStringList targetLanguages()
{
    return {QStringLiteral("zh-Hans"), QStringLiteral("zh-Hant"), QStringLiteral("ja"),
            QStringLiteral("en"),      QStringLiteral("ko"),      QStringLiteral("fr"),
            QStringLiteral("de"),      QStringLiteral("es"),      QStringLiteral("ru")};
}

QString languageName(const QString &code)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("zh-Hans"), QStringLiteral("简体中文")}, {QStringLiteral("zh-Hant"), QStringLiteral("繁體中文")},
        {QStringLiteral("ja"), QStringLiteral("日本語")},        {QStringLiteral("en"), QStringLiteral("English")},
        {QStringLiteral("ko"), QStringLiteral("한국어")},         {QStringLiteral("fr"), QStringLiteral("Français")},
        {QStringLiteral("de"), QStringLiteral("Deutsch")},       {QStringLiteral("es"), QStringLiteral("Español")},
        {QStringLiteral("ru"), QStringLiteral("Русский")}};
    return names.value(code, code);
}

QString defaultTarget()
{
    const QString ui = AppSettings::language().isEmpty() ? QLocale::system().name() : AppSettings::language();
    if (ui.startsWith(QLatin1String("zh")))
        return QStringLiteral("zh-Hans");
    if (ui.startsWith(QLatin1String("ja")))
        return QStringLiteral("ja");
    return QStringLiteral("en");
}

bool needsKey(const QString &engine)
{
    return engine != QLatin1String("local") && engine != QLatin1String("firefox")
           && engine != QLatin1String("google-free");
}

void translate(const QString &engine, const QStringList &texts, const QString &target, QObject *context,
               Callback done)
{
    if (engine == QLatin1String("google-free")) {
        googleFree(texts, target, context, done);
        return;
    }
    if (engine == QLatin1String("firefox")) {
        // The language files this text needs, fetched first (with consent).
        QString unsupported;
        const QList<ModelFile> missing = FirefoxTranslation::missingModels(texts, target, &unsupported);
        if (!unsupported.isEmpty()) {
            done({}, unsupported);
            return;
        }
        if (!missing.isEmpty()) {
            QStringList names;
            for (const ModelFile &file : missing)
                names << file.name;
            const auto answer = QMessageBox::question(
                nullptr, engineName(engine),
                QCoreApplication::translate("Translate", "Firefox translation needs these language files first (about "
                                                         "50 MB per direction):\n\n%1\n\nDownload them now? They are "
                                                         "kept on this computer and used offline from then on.")
                    .arg(names.join(QLatin1Char('\n'))));
            if (answer != QMessageBox::Yes || !ModelStore::download(missing, nullptr)) {
                done({}, QCoreApplication::translate("Translate", "The Firefox language files were not downloaded."));
                return;
            }
        }
        using Result = QPair<QStringList, QString>;
        auto *watcher = new QFutureWatcher<Result>(context);
        QObject::connect(watcher, &QFutureWatcher<Result>::finished, context, [watcher, done] {
            const Result result = watcher->result();
            watcher->deleteLater();
            done(result.first, result.second);
            static QTimer *idle = [] {
                auto *timer = new QTimer(QCoreApplication::instance());
                timer->setSingleShot(true);
                timer->setInterval(5 * 60 * 1000);
                QObject::connect(timer, &QTimer::timeout, [] { (void)QtConcurrent::run(&FirefoxTranslation::release); });
                return timer;
            }();
            idle->start();
        });
        watcher->setFuture(QtConcurrent::run([texts, target] {
            QString error;
            const QStringList translations = FirefoxTranslation::translate(texts, target, &error);
            return Result(translations, error);
        }));
        return;
    }
    if (engine == QLatin1String("local")) {
        // Inference blocks for seconds: run it on a worker thread.
        using Result = QPair<QStringList, QString>;
        auto *watcher = new QFutureWatcher<Result>(context);
        QObject::connect(watcher, &QFutureWatcher<Result>::finished, context, [watcher, done] {
            const Result result = watcher->result();
            watcher->deleteLater();
            done(result.first, result.second);
            // Keep the model loaded for the next capture, but not forever:
            // it holds gigabytes of (video) memory.
            static QTimer *idle = [] {
                auto *timer = new QTimer(QCoreApplication::instance());
                timer->setSingleShot(true);
                timer->setInterval(5 * 60 * 1000);
                QObject::connect(timer, &QTimer::timeout, [] { (void)QtConcurrent::run(&LocalModel::release); });
                return timer;
            }();
            idle->start();
        });
        const QString language = languageName(target);
        watcher->setFuture(QtConcurrent::run([texts, language] {
            QString error;
            const QStringList translations = LocalModel::translate(texts, language, &error);
            return Result(translations, error);
        }));
        return;
    }

    const QString key = AppSettings::translateKey(engine).trimmed();
    if (key.isEmpty()) {
        done({}, QCoreApplication::translate("Translate", "No API key is set for %1. Add one in Settings > Translation.").arg(engineName(engine)));
        return;
    }
    if (engine == QLatin1String("deepl"))
        deepl(texts, target, key, context, done);
    else if (engine == QLatin1String("google"))
        google(texts, target, key, context, done);
    else if (engine == QLatin1String("microsoft"))
        microsoft(texts, target, key, context, done);
    else if (engine == QLatin1String("claude"))
        claude(texts, target, key, context, done);
    else
        done({}, QCoreApplication::translate("Translate", "Unknown translation engine: %1").arg(engine));
}

} // namespace Translate
