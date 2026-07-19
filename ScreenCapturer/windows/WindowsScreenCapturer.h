/*---------------------------------------------------------*\
| WindowsScreenCapturer.h                                   |
|                                                           |
|   OpenRGB Effects Plugin Windows Screen Capturer          |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "ScreenCapturer.h"
#include <thread>
#include <QScreen>
#include <QPixmap>

class WindowsScreenCapturer : public ScreenCapturer
{
public:
    WindowsScreenCapturer();
    ~WindowsScreenCapturer();

    void Start() override;
    void Stop() override;
    void SetScreen(int) override;

private:
    void CaptureThreadFunction();
    std::thread* capture_thread = nullptr;
    bool continue_capture = false;
    QScreen* screen = nullptr;

    QPixmap grabWindow(quintptr window) const;
};
