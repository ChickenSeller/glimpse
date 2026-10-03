#pragma once

enum class CaptureMode {
    Window,
    Region,
    FullScreen,
};

inline constexpr CaptureMode kAllCaptureModes[] = {
    CaptureMode::Window,
    CaptureMode::Region,
    CaptureMode::FullScreen,
};
