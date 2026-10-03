#pragma once

enum class CaptureMode {
    Window,
    Region,
    FullScreen,
    QrCode, // select a region, decode the QR codes / barcodes in it
    Ocr,    // select a region, recognize the text in it
};

inline constexpr CaptureMode kAllCaptureModes[] = {
    CaptureMode::Window,
    CaptureMode::Region,
    CaptureMode::FullScreen,
    CaptureMode::QrCode,
    CaptureMode::Ocr,
};
