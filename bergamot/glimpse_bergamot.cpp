#include "glimpse_bergamot.h"

#include "translator/parser.h"
#include "translator/response_options.h"
#include "translator/service.h"
#include "translator/translation_model.h"

#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace mb = marian::bergamot;

struct gb_model {
    std::shared_ptr<mb::TranslationModel> model;
};

namespace {

void setError(char *err, int size, const std::string &message)
{
    if (!err || size <= 0)
        return;
    std::strncpy(err, message.c_str(), size_t(size) - 1);
    err[size - 1] = '\0';
}

// One service for the process; it batches and runs on the calling thread.
mb::BlockingService &service()
{
    static mb::BlockingService instance{mb::BlockingService::Config{}};
    return instance;
}

// YAML strings need quoting for Windows paths.
std::string quoted(const char *path)
{
    std::string out = "\"";
    for (const char *p = path; *p; ++p) {
        if (*p == '"' || *p == '\\')
            out += '\\';
        out += *p;
    }
    return out + "\"";
}

} // namespace

extern "C" {

gb_model *gb_model_load(const char *model, const char *src_vocab, const char *trg_vocab, const char *lex, char *err,
                        int err_size)
{
    try {
        // The settings Firefox's translation worker uses.
        std::string config = "models:\n  - " + quoted(model) + "\nvocabs:\n  - " + quoted(src_vocab) + "\n  - "
                             + quoted(trg_vocab ? trg_vocab : src_vocab) + "\n";
        if (lex && *lex)
            config += "shortlist:\n  - " + quoted(lex) + "\n  - false\n";
        config += "beam-size: 1\n"
                  "normalize: 1.0\n"
                  "word-penalty: 0\n"
                  "max-length-break: 128\n"
                  "mini-batch-words: 1024\n"
                  "workspace: 128\n"
                  "max-length-factor: 2.0\n"
                  "skip-cost: true\n"
                  "cpu-threads: 0\n"
                  "quiet: true\n"
                  "quiet-translation: true\n"
                  "gemm-precision: int8shiftAlphaAll\n"
                  "alignment: soft\n";
        auto options = mb::parseOptionsFromString(config, /*validate=*/false);
        return new gb_model{std::make_shared<mb::TranslationModel>(options)};
    } catch (const std::exception &e) {
        setError(err, err_size, e.what());
    } catch (...) {
        setError(err, err_size, "unknown error while loading the model");
    }
    return nullptr;
}

void gb_model_free(gb_model *model)
{
    delete model;
}

int gb_translate(gb_model *first, gb_model *second, const char *const *texts, int n, char **out, char *err,
                 int err_size)
{
    try {
        std::vector<std::string> sources;
        sources.reserve(size_t(n));
        for (int i = 0; i < n; ++i)
            sources.emplace_back(texts[i] ? texts[i] : "");
        const std::vector<mb::ResponseOptions> options(static_cast<size_t>(n));
        std::vector<mb::Response> responses =
            second ? service().pivotMultiple(first->model, second->model, std::move(sources), options)
                   : service().translateMultiple(first->model, std::move(sources), options);
        for (int i = 0; i < n; ++i) {
            const std::string &text = responses[size_t(i)].target.text;
            out[i] = new char[text.size() + 1];
            std::memcpy(out[i], text.c_str(), text.size() + 1);
        }
        return 0;
    } catch (const std::exception &e) {
        setError(err, err_size, e.what());
    } catch (...) {
        setError(err, err_size, "unknown error while translating");
    }
    return 1;
}

void gb_string_free(char *text)
{
    delete[] text;
}

} // extern "C"
