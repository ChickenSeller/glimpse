#pragma once

#include "capture/Platform.h"

#include <QtGlobal>

enum class CaptureMode {
    Window, // hover and click a window or control, or drag a rectangle
    FullScreen,
    QrCode, // select a region, decode the QR codes / barcodes in it
    Ocr,    // select a region, recognize the text in it (and translate it if asked)
    Scrolling, // pick a scrollable area, scroll it and stitch a tall image
    Freehand,  // draw any outline; the outside of it is left transparent
    Crosshair,   // full-screen crosshair with position, color and distances; picks a color too
    Recording,   // record a screen area to an MP4 video
    Pin,         // select a region and pin it to the screen, on top of everything
};

// The modes offered in the UI (tray menu, hotkeys, settings).
inline constexpr CaptureMode kAllCaptureModes[] = {
    CaptureMode::Window,
    CaptureMode::Scrolling,
    CaptureMode::Freehand,
    CaptureMode::FullScreen,
    CaptureMode::Pin,
    CaptureMode::Recording,
    CaptureMode::QrCode,
    CaptureMode::Ocr,
    CaptureMode::Crosshair,
};

// Whether this platform can do `mode` (the UI disables the others).
inline bool captureModeSupported(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Scrolling: return Platform::supportsScrollingCapture();
    default: return true;
    }
}

// The mode's name in menus and settings (translated in the "CaptureController"
// context: QCoreApplication::translate("CaptureController", label)).
inline const char *captureModeLabel(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Window: return QT_TRANSLATE_NOOP("CaptureController", "Capture Window / Region");
    case CaptureMode::FullScreen: return QT_TRANSLATE_NOOP("CaptureController", "Capture Full Screen");
    case CaptureMode::QrCode: return QT_TRANSLATE_NOOP("CaptureController", "Scan QR Code");
    case CaptureMode::Ocr: return QT_TRANSLATE_NOOP("CaptureController", "Recognize and Translate Text");
    case CaptureMode::Scrolling: return QT_TRANSLATE_NOOP("CaptureController", "Scrolling Capture (Experimental)");
    case CaptureMode::Freehand: return QT_TRANSLATE_NOOP("CaptureController", "Capture Freehand Region");
    case CaptureMode::Crosshair: return QT_TRANSLATE_NOOP("CaptureController", "Crosshair / Color Picker");
    case CaptureMode::Recording: return QT_TRANSLATE_NOOP("CaptureController", "Record Screen");
    case CaptureMode::Pin: return QT_TRANSLATE_NOOP("CaptureController", "Pin Region to Screen");
    }
    return "";
}

// The mode's toolbar icon (the same as in CaptureToolbar.ui).
inline const char *captureModeIcon(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Window: return ":/icons/screenshot_region.svg";
    case CaptureMode::FullScreen: return ":/icons/screenshot_monitor.svg";
    case CaptureMode::QrCode: return ":/icons/qr_code_scanner.svg";
    case CaptureMode::Ocr: return ":/icons/document_scanner.svg";
    case CaptureMode::Scrolling: return ":/icons/fit_page_height.svg";
    case CaptureMode::Freehand: return ":/icons/lasso_select.svg";
    case CaptureMode::Crosshair: return ":/icons/my_location.svg";
    case CaptureMode::Recording: return ":/icons/videocam.svg";
    case CaptureMode::Pin: return ":/icons/push_pin.svg";
    }
    return "";
}
