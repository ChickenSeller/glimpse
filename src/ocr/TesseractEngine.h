#pragma once

#include "OcrEngine.h"

#include <memory>

namespace tesseract {
class TessBaseAPI;
}

// Tesseract 5 (LSTM) with the "fast" trained models from tessdata_fast,
// downloaded into the user's data directory.
class TesseractEngine : public OcrEngine
{
public:
    ~TesseractEngine() override;

    static QList<ModelFile> requiredModels(const QStringList &languages);
    static std::unique_ptr<OcrEngine> create(const QStringList &languages, QString *error);

    OcrResult recognize(const QImage &image) override;

private:
    TesseractEngine();

    std::unique_ptr<tesseract::TessBaseAPI> m_api;
};
