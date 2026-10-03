#pragma once

#include "ScreenGrabber.h"

// QScreen::grabWindow(0) backend, used on Windows and X11.
class QtScreenGrabber : public ScreenGrabber
{
    Q_OBJECT

public:
    using ScreenGrabber::ScreenGrabber;

    void grab() override;
};
