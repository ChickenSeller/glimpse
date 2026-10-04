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

// The model file the settings select (may not exist yet).
QString modelPath();
// The preset to download for the current settings, if one is selected and missing.
std::optional<Preset> missingPreset();

// Why local translation cannot run in this build; empty if it can.
QString unavailableReason();

// Translates each text into `language` (its name, e.g. "日本語"). Blocking:
// call it off the GUI thread. Calls are serialized; the model stays loaded
// between them until release().
QStringList translate(const QStringList &texts, const QString &language, QString *error);
// Frees the model (and its GPU memory) if it is loaded and not in use.
void release();

} // namespace LocalModel
