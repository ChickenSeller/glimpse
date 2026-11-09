#pragma once

#include "ocr/OcrEngine.h" // ModelFile

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

// Offline translation with a GGUF language model run by llama.cpp (loaded at
// run time). Glimpse offers a few models to download; any GGUF chat model
// can be chosen instead.
namespace LocalModel {

struct Preset {
    QString id;       // "qwen3-4b", "hunyuan-mt-7b"
    QString name;     // shown in the settings
    ModelFile file;   // where it is fetched from and stored
};

QList<Preset> presets();
QString defaultPreset();
// "custom" selects the file from AppSettings::localModelFile().
inline constexpr char kCustom[] = "custom";

// `model` below is a preset id or kCustom; empty means the one the settings
// select.

// The model file (may not exist yet).
QString modelPath(const QString &model = QString());
// The preset to download, if the model is one and missing.
std::optional<Preset> missingPreset(const QString &model = QString());
// The model's name for the UI: the preset's, or the custom file's.
QString modelName(const QString &model);

// Why local translation cannot run in this build; empty if it can.
QString unavailableReason();

// Translates each text into `language` (its name, e.g. "日本語"). Blocking:
// call it off the GUI thread. Calls are serialized; the model stays loaded
// between them until release().
QStringList translate(const QStringList &texts, const QString &language, QString *error,
                      const QString &model = QString());
// Frees the model (and its GPU memory) if it is loaded and not in use.
void release();

} // namespace LocalModel
