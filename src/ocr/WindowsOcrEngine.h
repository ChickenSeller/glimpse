#pragma once

#include "OcrEngine.h"

// Windows.Media.Ocr, built into Windows 10+. Needs no downloads, but only
// recognizes languages whose OCR component is installed (Settings > Time &
// language > Language). MinGW ships no headers for it, so the few WinRT
// interfaces used are declared by hand in the .cpp.
class WindowsOcrEngine : public OcrEngine
{
public:
    ~WindowsOcrEngine() override;

    static std::unique_ptr<OcrEngine> create(const QStringList &languages, QString *error);

    OcrResult recognize(const QImage &image) override;

private:
    WindowsOcrEngine() = default;

    struct Private;
    std::unique_ptr<Private> d;
};
