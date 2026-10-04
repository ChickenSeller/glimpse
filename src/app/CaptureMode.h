#pragma once

#include "capture/Platform.h"

#include <QtGlobal>

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
    Pin,         // select a region and pin it to the screen, on top of everything
};

// The modes offered in the UI (tray menu, hotkeys, settings). Scrolling
// capture is left out for now: it is not yet reliable on real web pages. Its
// toolbar button and settings row are hidden as well (see kScrollingHidden).
inline constexpr CaptureMode kAllCaptureModes[] = {
    CaptureMode::Window,
    CaptureMode::Region,
    CaptureMode::Freehand,
    CaptureMode::FullScreen,
    CaptureMode::Pin,
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

// The mode's name in menus and settings (translated in the "CaptureController"
// context: QCoreApplication::translate("CaptureController", label)).
inline const char *captureModeLabel(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Window: return QT_TRANSLATE_NOOP("CaptureController", "Capture Window / Object");
    case CaptureMode::Region: return QT_TRANSLATE_NOOP("CaptureController", "Capture Rectangular Region");
    case CaptureMode::FullScreen: return QT_TRANSLATE_NOOP("CaptureController", "Capture Full Screen");
    case CaptureMode::QrCode: return QT_TRANSLATE_NOOP("CaptureController", "Scan QR Code");
    case CaptureMode::Ocr: return QT_TRANSLATE_NOOP("CaptureController", "Recognize Text");
    case CaptureMode::Scrolling: return QT_TRANSLATE_NOOP("CaptureController", "Scrolling Capture");
    case CaptureMode::Freehand: return QT_TRANSLATE_NOOP("CaptureController", "Capture Freehand Region");
    case CaptureMode::ColorPicker: return QT_TRANSLATE_NOOP("CaptureController", "Pick Screen Color");
    case CaptureMode::Crosshair: return QT_TRANSLATE_NOOP("CaptureController", "Screen Crosshair");
    case CaptureMode::Recording: return QT_TRANSLATE_NOOP("CaptureController", "Record Screen");
    case CaptureMode::Translate: return QT_TRANSLATE_NOOP("CaptureController", "Translate Screenshot");
    case CaptureMode::Pin: return QT_TRANSLATE_NOOP("CaptureController", "Pin Region to Screen");
    }
    return "";
}

// The mode's toolbar icon (the same as in CaptureToolbar.ui).
inline const char *captureModeIcon(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Window: return ":/icons/select_window.svg";
    case CaptureMode::Region: return ":/icons/screenshot_region.svg";
    case CaptureMode::FullScreen: return ":/icons/screenshot_monitor.svg";
    case CaptureMode::QrCode: return ":/icons/qr_code_scanner.svg";
    case CaptureMode::Ocr: return ":/icons/document_scanner.svg";
    case CaptureMode::Scrolling: return ":/icons/swipe_vertical.svg";
    case CaptureMode::Freehand: return ":/icons/lasso_select.svg";
    case CaptureMode::ColorPicker: return ":/icons/colorize.svg";
    case CaptureMode::Crosshair: return ":/icons/my_location.svg";
    case CaptureMode::Recording: return ":/icons/videocam.svg";
    case CaptureMode::Translate: return ":/icons/translate.svg";
    case CaptureMode::Pin: return ":/icons/push_pin.svg";
    }
    return "";
}
