#include "LocalModel.h"

#include "app/AppSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

#ifdef GLIMPSE_HAVE_LLAMA
#include <ggml-backend.h>
#include <llama.h>
#endif

namespace {

// Large files: kept per machine (not in the roaming profile).
QString modelsDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("models"));
}

} // namespace

namespace LocalModel {

QList<Preset> presets()
{
    const QString dir = modelsDirectory();
    const auto preset = [&dir](const char *id, const QString &name, const char *file, const char *url) {
        return Preset{QString::fromLatin1(id), name,
                      ModelFile{QString::fromLatin1(file), QUrl(QString::fromLatin1(url)),
                                QDir(dir).filePath(QString::fromLatin1(file))}};
    };
    // Tencent's Hy-MT2 models are made for translation (33 languages, Apache 2.0).
    return {
        preset("hy-mt2-1.8b", QCoreApplication::translate("LocalModel", "Hy-MT2 1.8B (1.1 GB, light: runs on any laptop)"),
               "Hy-MT2-1.8B-Q4_K_M.gguf",
               "https://huggingface.co/tencent/Hy-MT2-1.8B-GGUF/resolve/main/Hy-MT2-1.8B-Q4_K_M.gguf"),
        preset("qwen3-4b", QCoreApplication::translate("LocalModel", "Qwen3 4B (2.5 GB, general model)"),
               "Qwen3-4B-Q4_K_M.gguf",
               "https://huggingface.co/Qwen/Qwen3-4B-GGUF/resolve/main/Qwen3-4B-Q4_K_M.gguf"),
        preset("hy-mt2-7b", QCoreApplication::translate("LocalModel", "Hy-MT2 7B (4.6 GB, best; wants a graphics card)"),
               "Hy-MT2-7B-Q4_K_M.gguf",
               "https://huggingface.co/tencent/Hy-MT2-7B-GGUF/resolve/main/Hy-MT2-7B-Q4_K_M.gguf"),
    };
}

QString defaultPreset()
{
    // The light model: fast enough on an office laptop without a graphics card.
    return QStringLiteral("hy-mt2-1.8b");
}

QString modelPath()
{
    const QString selected = AppSettings::localModel();
    if (selected == QLatin1String(kCustom))
        return AppSettings::localModelFile();
    const QList<Preset> all = presets();
    for (const Preset &p : all) {
        if (p.id == selected)
            return p.file.path;
    }
    return all.first().file.path;
}

std::optional<Preset> missingPreset()
{
    const QString selected = AppSettings::localModel();
    if (selected == QLatin1String(kCustom))
        return std::nullopt;
    const QList<Preset> all = presets();
    Preset chosen = all.first();
    for (const Preset &p : all) {
        if (p.id == selected)
            chosen = p;
    }
    return QFileInfo::exists(chosen.file.path) ? std::nullopt : std::optional<Preset>(chosen);
}

#ifndef GLIMPSE_HAVE_LLAMA

QString unavailableReason()
{
    return QCoreApplication::translate("LocalModel", "This build does not include llama.cpp.");
}

QStringList translate(const QStringList &, const QString &, QString *error)
{
    *error = unavailableReason();
    return {};
}

void release() {}

#else

namespace {

// The llama.cpp C API, resolved from the libraries next to the executable.
struct Api {
    QLibrary llama;
    QLibrary ggml;
    decltype(&ggml_backend_load_all) backendLoadAll = nullptr;
    decltype(&llama_backend_init) backendInit = nullptr;
    decltype(&llama_log_set) logSet = nullptr;
    decltype(&llama_model_default_params) modelDefaultParams = nullptr;
    decltype(&llama_context_default_params) contextDefaultParams = nullptr;
    decltype(&llama_sampler_chain_default_params) chainDefaultParams = nullptr;
    decltype(&llama_model_load_from_file) modelLoad = nullptr;
    decltype(&llama_model_free) modelFree = nullptr;
    decltype(&llama_init_from_model) contextInit = nullptr;
    decltype(&llama_free) contextFree = nullptr;
    decltype(&llama_n_ctx) nCtx = nullptr;
    decltype(&llama_get_memory) getMemory = nullptr;
    decltype(&llama_memory_clear) memoryClear = nullptr;
    decltype(&llama_model_get_vocab) modelVocab = nullptr;
    decltype(&llama_model_chat_template) chatTemplate = nullptr;
    decltype(&llama_chat_apply_template) applyTemplate = nullptr;
    decltype(&llama_tokenize) tokenize = nullptr;
    decltype(&llama_token_to_piece) tokenToPiece = nullptr;
    decltype(&llama_vocab_is_eog) isEog = nullptr;
    decltype(&llama_batch_get_one) batchGetOne = nullptr;
    decltype(&llama_decode) decode = nullptr;
    decltype(&llama_sampler_chain_init) chainInit = nullptr;
    decltype(&llama_sampler_chain_add) chainAdd = nullptr;
    decltype(&llama_sampler_init_greedy) greedy = nullptr;
    decltype(&llama_sampler_sample) sample = nullptr;
    decltype(&llama_sampler_free) samplerFree = nullptr;

    bool load(QString *error)
    {
        if (modelLoad)
            return true;
        const QString dir = QCoreApplication::applicationDirPath();
        llama.setFileName(QDir(dir).filePath(QStringLiteral("llama")));
        ggml.setFileName(QDir(dir).filePath(QStringLiteral("ggml")));
        if (!llama.load() || !ggml.load()) {
            *error = QCoreApplication::translate("LocalModel", "llama.cpp could not be loaded: %1").arg(llama.isLoaded() ? ggml.errorString() : llama.errorString());
            return false;
        }
        bool ok = true;
        const auto get = [&ok](QLibrary &lib, auto &fn, const char *name) {
            fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(lib.resolve(name));
            ok = ok && fn;
        };
        get(ggml, backendLoadAll, "ggml_backend_load_all");
        get(llama, backendInit, "llama_backend_init");
        get(llama, logSet, "llama_log_set");
        get(llama, modelDefaultParams, "llama_model_default_params");
        get(llama, contextDefaultParams, "llama_context_default_params");
        get(llama, chainDefaultParams, "llama_sampler_chain_default_params");
        get(llama, modelLoad, "llama_model_load_from_file");
        get(llama, modelFree, "llama_model_free");
        get(llama, contextInit, "llama_init_from_model");
        get(llama, contextFree, "llama_free");
        get(llama, nCtx, "llama_n_ctx");
        get(llama, getMemory, "llama_get_memory");
        get(llama, memoryClear, "llama_memory_clear");
        get(llama, modelVocab, "llama_model_get_vocab");
        get(llama, chatTemplate, "llama_model_chat_template");
        get(llama, applyTemplate, "llama_chat_apply_template");
        get(llama, tokenize, "llama_tokenize");
        get(llama, tokenToPiece, "llama_token_to_piece");
        get(llama, isEog, "llama_vocab_is_eog");
        get(llama, batchGetOne, "llama_batch_get_one");
        get(llama, decode, "llama_decode");
        get(llama, chainInit, "llama_sampler_chain_init");
        get(llama, chainAdd, "llama_sampler_chain_add");
        get(llama, greedy, "llama_sampler_init_greedy");
        get(llama, sample, "llama_sampler_sample");
        get(llama, samplerFree, "llama_sampler_free");
        if (!ok) {
            modelLoad = nullptr;
            *error = QCoreApplication::translate("LocalModel", "This llama.cpp library is a different version than Glimpse expects.");
            return false;
        }
        // Only errors; llama.cpp is chatty on stderr otherwise.
        logSet([](ggml_log_level level, const char *text, void *) {
            if (level == GGML_LOG_LEVEL_ERROR)
                qWarning("llama.cpp: %s", text);
        }, nullptr);
        backendLoadAll(); // GPU (Vulkan) and the best CPU backend, from next to the executable
        backendInit();
        return true;
    }
};

constexpr uint32_t kContext = 4096;

std::mutex g_mutex; // one translation at a time; guards everything below
Api g_api;
llama_model *g_model = nullptr;
llama_context *g_context = nullptr;
llama_sampler *g_sampler = nullptr;
QString g_loadedPath;

void unloadLocked()
{
    if (g_sampler)
        g_api.samplerFree(g_sampler);
    if (g_context)
        g_api.contextFree(g_context);
    if (g_model)
        g_api.modelFree(g_model);
    g_sampler = nullptr;
    g_context = nullptr;
    g_model = nullptr;
    g_loadedPath.clear();
}

bool ensureModel(const QString &path, QString *error)
{
    if (!g_api.load(error))
        return false;
    if (g_model && g_loadedPath == path)
        return true;
    unloadLocked();
    if (!QFileInfo::exists(path)) {
        *error = QCoreApplication::translate("LocalModel", "The model file does not exist: %1").arg(QDir::toNativeSeparators(path));
        return false;
    }
    llama_model_params modelParams = g_api.modelDefaultParams();
    // Everything the GPU can take. GLIMPSE_LLAMA_GPU_LAYERS=0 forces the CPU,
    // e.g. to see how an office laptop without a graphics card would fare.
    bool overridden = false;
    const int gpuLayers = qEnvironmentVariableIntValue("GLIMPSE_LLAMA_GPU_LAYERS", &overridden);
    modelParams.n_gpu_layers = overridden ? gpuLayers : 999;
    g_model = g_api.modelLoad(QFile::encodeName(path).constData(), modelParams);
    if (!g_model) {
        *error = QCoreApplication::translate("LocalModel", "The model could not be loaded (is it a GGUF model llama.cpp supports?): %1")
                     .arg(QDir::toNativeSeparators(path));
        return false;
    }
    llama_context_params contextParams = g_api.contextDefaultParams();
    contextParams.n_ctx = kContext;
    contextParams.n_batch = 1024;
    // On the CPU: reading the prompt uses every core; generating is memory
    // bound and does best with about one thread per physical core.
    const int cores = std::max(1, QThread::idealThreadCount());
    contextParams.n_threads = std::clamp(cores / 2, 1, 16);
    contextParams.n_threads_batch = cores;
    g_context = g_api.contextInit(g_model, contextParams);
    if (!g_context) {
        unloadLocked();
        *error = QCoreApplication::translate("LocalModel", "Not enough memory to run the model.");
        return false;
    }
    // Greedy decoding: translations should be deterministic.
    g_sampler = g_api.chainInit(g_api.chainDefaultParams());
    g_api.chainAdd(g_sampler, g_api.greedy());
    g_loadedPath = path;
    return true;
}

// The chat messages for one paragraph, in the form the model was trained on.
std::string buildPrompt(const QString &text, const QString &language, const QString &modelFile)
{
    const QByteArray userText = text.toUtf8();
    QByteArray system;
    QByteArray user;
    if (modelFile.contains(QLatin1String("hy-mt"), Qt::CaseInsensitive)
        || modelFile.contains(QLatin1String("hunyuan-mt"), Qt::CaseInsensitive)) {
        // The Hy-MT models' documented prompts (Chinese ones for Chinese);
        // they take no system message.
        user = language.contains(QStringLiteral("中文"))
                   ? QStringLiteral("将以下文本翻译为%1，注意只需要输出翻译后的结果，不要额外解释：\n\n").arg(language).toUtf8()
                         + userText
                   : QStringLiteral("Translate the following text into %1. Note that you should only output the "
                                    "translated result without any additional explanation:\n\n")
                             .arg(language)
                             .toUtf8()
                         + userText;
    } else {
        system = QStringLiteral(
                     "You are a translation engine. Translate the user's text into %1. The text was recognized from "
                     "a screenshot and may contain OCR mistakes; read through them. Keep line breaks, numbers and "
                     "names. Reply with the translation only, without notes or quotes.")
                     .arg(language)
                     .toUtf8();
        user = userText;
        // Qwen3 thinks before answering unless told not to.
        if (modelFile.contains(QLatin1String("qwen3"), Qt::CaseInsensitive))
            user += "\n/no_think";
    }

    std::vector<llama_chat_message> messages;
    if (!system.isEmpty())
        messages.push_back({"system", system.constData()});
    messages.push_back({"user", user.constData()});

    const char *tmpl = g_api.chatTemplate(g_model, nullptr);
    std::string buffer(size_t(system.size() + user.size()) * 2 + 1024, '\0');
    int32_t n = g_api.applyTemplate(tmpl, messages.data(), messages.size(), true, buffer.data(), int32_t(buffer.size()));
    if (n > int32_t(buffer.size())) {
        buffer.resize(size_t(n));
        n = g_api.applyTemplate(tmpl, messages.data(), messages.size(), true, buffer.data(), int32_t(buffer.size()));
    }
    if (n > 0) {
        buffer.resize(size_t(n));
        return buffer;
    }
    // A template llama.cpp does not know: ChatML, which most models accept.
    std::string chatml;
    if (!system.isEmpty())
        chatml += "<|im_start|>system\n" + system.toStdString() + "<|im_end|>\n";
    chatml += "<|im_start|>user\n" + user.toStdString() + "<|im_end|>\n<|im_start|>assistant\n";
    return chatml;
}

QString translateOne(const QString &text, const QString &language, QString *error)
{
    const llama_vocab *vocab = g_api.modelVocab(g_model);
    const std::string prompt = buildPrompt(text, language, QFileInfo(g_loadedPath).fileName());

    std::vector<llama_token> tokens(prompt.size() + 16);
    int32_t count = g_api.tokenize(vocab, prompt.data(), int32_t(prompt.size()), tokens.data(),
                                   int32_t(tokens.size()), true, true);
    if (count < 0) {
        tokens.resize(size_t(-count));
        count = g_api.tokenize(vocab, prompt.data(), int32_t(prompt.size()), tokens.data(), int32_t(tokens.size()),
                               true, true);
    }
    const int32_t context = int32_t(g_api.nCtx(g_context));
    if (count <= 0 || count > context - 256) {
        *error = QCoreApplication::translate("LocalModel", "A paragraph is too long for the local model; split it with blank lines.");
        return {};
    }
    tokens.resize(size_t(count));

    g_api.memoryClear(g_api.getMemory(g_context), true);
    for (int32_t i = 0; i < count; i += 1024) {
        const int32_t chunk = std::min<int32_t>(1024, count - i);
        if (g_api.decode(g_context, g_api.batchGetOne(tokens.data() + i, chunk)) != 0) {
            *error = QCoreApplication::translate("LocalModel", "The local model failed to read the text.");
            return {};
        }
    }

    std::string output;
    const int32_t limit = std::min(context - count - 8, 2048);
    for (int32_t n = 0; n < limit; ++n) {
        llama_token token = g_api.sample(g_sampler, g_context, -1);
        if (g_api.isEog(vocab, token))
            break;
        char piece[256];
        // Special tokens rendered too, so that a thinking block arrives whole
        // (Qwen3's </think> is one) and can be removed below.
        const int32_t length = g_api.tokenToPiece(vocab, token, piece, sizeof(piece), 0, true);
        if (length > 0)
            output.append(piece, size_t(length));
        if (g_api.decode(g_context, g_api.batchGetOne(&token, 1)) != 0)
            break;
    }

    QString result = QString::fromUtf8(output);
    // Reasoning models may still emit an (empty) thinking block first.
    static const QRegularExpression think(QStringLiteral("<think>.*?</think>"),
                                          QRegularExpression::DotMatchesEverythingOption);
    static const QRegularExpression strayTag(QStringLiteral("</?think>"));
    result.remove(think);
    result.remove(strayTag);
    return result.trimmed();
}

} // namespace

QString unavailableReason()
{
    const QString dir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const bool present = QFileInfo::exists(QDir(dir).filePath(QStringLiteral("llama.dll")));
#else
    const bool present = !QDir(dir).entryList({QStringLiteral("libllama.so*")}).isEmpty();
#endif
    return present ? QString() : QCoreApplication::translate("LocalModel", "The llama.cpp libraries are missing next to Glimpse.");
}

QStringList translate(const QStringList &texts, const QString &language, QString *error)
{
    std::lock_guard lock(g_mutex);
    if (!ensureModel(modelPath(), error))
        return {};
    QStringList results;
    for (const QString &text : texts) {
        results << translateOne(text, language, error);
        if (!error->isEmpty())
            return {};
    }
    return results;
}

void release()
{
    // A translation in progress keeps the model; it is released next time.
    std::unique_lock lock(g_mutex, std::try_to_lock);
    if (lock.owns_lock())
        unloadLocked();
}

#endif

} // namespace LocalModel
