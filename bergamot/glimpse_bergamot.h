/* glimpse-bergamot: a plain C interface to Mozilla's Bergamot translation
 * engine (the one behind Firefox's offline translations).
 *
 * The engine is C++ and builds with MSVC on Windows, while Glimpse uses
 * MinGW; a C interface is what both can share. Glimpse loads this library at
 * run time. All calls on one process must be serialized by the caller. */
#ifndef GLIMPSE_BERGAMOT_H
#define GLIMPSE_BERGAMOT_H

#ifdef _WIN32
#  ifdef GLIMPSE_BERGAMOT_BUILD
#    define GB_API __declspec(dllexport)
#  else
#    define GB_API __declspec(dllimport)
#  endif
#else
#  define GB_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gb_model gb_model;

/* Loads one translation direction from Firefox's model files (UTF-8 paths).
 * trg_vocab may equal src_vocab (shared vocabulary); lex may be NULL.
 * Returns NULL with a message in err on failure. */
GB_API gb_model *gb_model_load(const char *model, const char *src_vocab, const char *trg_vocab, const char *lex,
                               char *err, int err_size);
GB_API void gb_model_free(gb_model *model);

/* Translates n UTF-8 texts with `first`, then with `second` if it is not NULL
 * (pivoting, e.g. zh -> en -> ja). On success returns 0 and sets out[i] to
 * newly allocated UTF-8 strings, to be freed with gb_string_free. */
GB_API int gb_translate(gb_model *first, gb_model *second, const char *const *texts, int n, char **out, char *err,
                        int err_size);
GB_API void gb_string_free(char *text);

#ifdef __cplusplus
}
#endif

#endif
