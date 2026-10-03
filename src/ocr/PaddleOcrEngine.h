#pragma once

#include "OcrEngine.h"

// PaddleOCR (PP-OCRv6 small, official ONNX export) run through ONNX Runtime.
// Two stages: a DB text detector finds line boxes, a CTC recognizer reads each
// box. One recognition model covers Chinese, Japanese and English.
//
// ONNX Runtime is loaded at run time through its C API (OrtGetApiBase), so the
// build only needs its header and the engine reports itself unavailable when
// the library is missing.
class PaddleOcrEngine : public OcrEngine
{
public:
    ~PaddleOcrEngine() override;

    static QList<ModelFile> requiredModels();
    static std::unique_ptr<OcrEngine> create(QString *error);

    OcrResult recognize(const QImage &image) override;

private:
    PaddleOcrEngine() = default;

    struct Private;
    std::unique_ptr<Private> d;
};
