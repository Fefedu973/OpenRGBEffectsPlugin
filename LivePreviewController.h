/*---------------------------------------------------------*\
| LivePreviewController.h                                   |
|                                                           |
|   OpenRGB Effects Plugin Live Preview RGBController       |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <mutex>
#include <QWidget>
#include "RGBControllerInterface.h"
#include "ui_LivePreviewController.h"

namespace Ui
{
    class LivePreviewController;
}

class LivePreviewController : public QWidget
{
    Q_OBJECT

public:
    LivePreviewController(QWidget *parent = nullptr);
    ~LivePreviewController();

    void                        DeviceUpdateLEDs();
    void                        DeviceUpdateZoneLEDs(int);
    void                        DeviceUpdateMode();
    void                        DeviceUpdateSingleLED(int);

    RGBControllerInterface*     controller;
    
signals:
    void                        Rendered(QImage);
    void                        ReversedChanged(bool);

private slots:
    void                        changeEvent(QEvent *event) override;
    void                        on_presets_currentIndexChanged(int);
    void                        on_brightness_valueChanged(int);
    void                        on_width_valueChanged(int);
    void                        on_height_valueChanged(int);
    void                        on_reverse_stateChanged(int);
    void                        on_scale_stateChanged(int);
    void                        Draw(QImage);

private:
    Ui::LivePreviewController*  ui;

    struct ZonePreset
    {
        std::string             name;
        zone_type               zt;
        unsigned int            height;
        unsigned int            width;
    };

    const std::vector<ZonePreset> presets =
    {
        {"Matrix 8x8",          ZONE_TYPE_MATRIX, 8,   8},
        {"Matrix 8x16",         ZONE_TYPE_MATRIX, 8,   16},
        {"Matrix 8x32",         ZONE_TYPE_MATRIX, 8,   32},
        {"Matrix 8x64",         ZONE_TYPE_MATRIX, 8,   64},
        {"Matrix 16x16",        ZONE_TYPE_MATRIX, 16,  16},
        {"Matrix 16x32",        ZONE_TYPE_MATRIX, 16,  32},
        {"Matrix 16x64",        ZONE_TYPE_MATRIX, 16,  64},
        {"Matrix 32x32",        ZONE_TYPE_MATRIX, 32,  32},
        {"Matrix 32x64",        ZONE_TYPE_MATRIX, 32,  64},
        {"Matrix 64x64",        ZONE_TYPE_MATRIX, 64,  64},
        {"Matrix 64x128",       ZONE_TYPE_MATRIX, 64,  128},
        {"Matrix 128x128",      ZONE_TYPE_MATRIX, 128, 128},
        {"Linear 8 LEDs",       ZONE_TYPE_LINEAR, 8,   1},
        {"Linear 10 LEDs",      ZONE_TYPE_LINEAR, 10,  1},
        {"Linear 12 LEDs",      ZONE_TYPE_LINEAR, 12,  1},
        {"Linear 16 LEDs",      ZONE_TYPE_LINEAR, 16,  1},
        {"Linear 20 LEDs",      ZONE_TYPE_LINEAR, 20,  1},
        {"Linear 24 LEDs",      ZONE_TYPE_LINEAR, 24,  1},
        {"Linear 48 LEDs",      ZONE_TYPE_LINEAR, 48,  1},
        {"Linear 72 LEDs",      ZONE_TYPE_LINEAR, 72,  1},
        {"Linear 128 LEDs",     ZONE_TYPE_LINEAR, 128, 1}
    };

    RGBController_Setup         setup;
    unsigned int                width   = presets[0].width;
    unsigned int                height  = presets[0].height;
    std::mutex                  lock;

    void                        SetupZone(std::string, zone_type, unsigned int, unsigned int);
    void                        Update(std::string, zone_type);

    static void                 DeviceUpdateLEDs_func(void* object_ptr);
};
