#pragma once

#include "capture/Platform.h"

enum class CaptureMode {
    Window,
    Region,
    FullScreen,
    QrCode, // select a region, decode the QR codes / barcodes in it
    Ocr,    // select a region, recognize the text in it
    Scrolling, // pick a scrollable area, scroll it and stitch a tall image
};

// The modes offered in the UI (tray menu, hotkeys, settings). Scrolling
// capture is left out for now: it is not yet reliable on real web pages. Its
// toolbar button and settings row are hidden as well (see kScrollingHidden).
inline constexpr CaptureMode kAllCaptureModes[] = {
    CaptureMode::Window,
    CaptureMode::Region,
    CaptureMode::FullScreen,
    CaptureMode::QrCode,
    CaptureMode::Ocr,
};
inline constexpr bool kScrollingHidden = true;

// Whether this platform can do `mode` (the UI disables the others).
inline bool captureModeSupported(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Window: return Platform::supportsWindowPicking();
    case CaptureMode::Scrolling: return Platform::supportsScrollingCapture();
    default: return true;
    }
}
