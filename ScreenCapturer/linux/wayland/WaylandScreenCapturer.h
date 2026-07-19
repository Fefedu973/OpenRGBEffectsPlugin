/*---------------------------------------------------------*\
| WaylandScreenCapturer.h                                   |
|                                                           |
|   OpenRGB Effects Plugin Wayland Screen Capturer          |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "ScreenCapturer.h"
#include "DBusScreenCastManager.h"
#include "PipeWireCapturer.h"

class WaylandScreenCapturer : public ScreenCapturer
{
public:
    WaylandScreenCapturer(QObject* parent = nullptr);
    ~WaylandScreenCapturer();

    void Init(const QString& restore_token = "",  bool auto_start = false) override;
    void SetToken(const QString& restore_token = "") override;
    void Start() override;
    void Stop() override;
    void SetScreen(int) override;

private:
    DBusScreenCastManager *dbus_manager = nullptr;
    PipeWireCapturer* capturer = nullptr;
};
