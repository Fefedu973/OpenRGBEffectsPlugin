/*---------------------------------------------------------*\
| QtScreenCapturer.h                                        |
|                                                           |
|   OpenRGB Effects Plugin Qt Screen Capturer               |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "ScreenCapturer.h"
#include <thread>
#include <QScreen>

class QtScreenCapturer : public ScreenCapturer
{
public:
    QtScreenCapturer();
    ~QtScreenCapturer();

    void Start() override;
    void Stop() override;
    void SetScreen(int) override;

private:
    void CaptureThreadFunction();
    std::thread* capture_thread = nullptr;
    bool continue_capture = false;
    QScreen* screen = nullptr;
};
