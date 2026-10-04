#pragma once

#include "capture/Platform.h"

enum class CaptureMode {
    Window,
    Region,
    FullScreen,
    QrCode, // select a region, decode the QR codes / barcodes in it
    Ocr,    // select a region, recognize the text in it
    Scrolling, // pick a scrollable area, scroll it and stitch a tall image
    Freehand,  // draw any outline; the outside of it is left transparent
    ColorPicker, // pick a pixel's color from the frozen screen
    Crosshair,   // full-screen crosshair with position, color and distances
    Recording,   // record a screen area to an MP4 video
    Translate,   // select a region, recognize its text and translate it
};

// The modes offered in the UI (tray menu, hotkeys, settings). Scrolling
// capture is left out for now: it is not yet reliable on real web pages. Its
// toolbar button and settings row are hidden as well (see kScrollingHidden).
inline constexpr CaptureMode kAllCaptureModes[] = {
    CaptureMode::Window,
    CaptureMode::Region,
    CaptureMode::Freehand,
    CaptureMode::FullScreen,
    CaptureMode::Recording,
    CaptureMode::QrCode,
    CaptureMode::Ocr,
    CaptureMode::Translate,
    CaptureMode::ColorPicker,
    CaptureMode::Crosshair,
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
