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
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

class WindowsScreenCapturer : public ScreenCapturer
{
public:
    WindowsScreenCapturer();
    ~WindowsScreenCapturer() override;

    void Start() override;
    void Stop() override;
    void SetScreen(int) override;

private:
    void CaptureThreadFunction();
    std::thread capture_thread;
    std::atomic<bool> continue_capture{false};
    std::mutex lifecycle_mutex;
    std::mutex target_mutex;
    std::condition_variable wake;
    QString device_name;
    std::atomic<std::uint64_t> target_revision{0};
};
