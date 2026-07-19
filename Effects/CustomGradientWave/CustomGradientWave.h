/*---------------------------------------------------------*\
| CustomGradientWave.h                                      |
|                                                           |
|   OpenRGB Effects Plugin Custom Gradient Wave Effect      |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include <QWidget>
#include "ui_CustomGradientWave.h"
#include "ColorUtils.h"

enum
{
    HORIZONTAL = 0,
    VERTICAL   = 1,
    RADIAL_OUT = 2,
    RADIAL_IN  = 3
};

typedef struct
{
    const char * name;
    std::vector<RGBColor> colors;

} CustomGradientWavePreset;

namespace Ui {
class CustomGradientWave;
}

class CustomGradientWave : public RGBEffect
{
    Q_OBJECT

public:
    explicit CustomGradientWave(QWidget *parent = nullptr);
    ~CustomGradientWave();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_RAINBOW, [](){return new CustomGradientWave;});

    static std::string const ClassName() {return "CustomGradientWave";}
    static std::string const UI_Name() { return QT_TR_NOOP("Custom Gradient Wave"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_preset_currentIndexChanged(int);
    void on_spread_valueChanged(int);
    void on_direction_currentIndexChanged(int);
    void on_height_valueChanged(int);
    void on_width_valueChanged(int);
    void on_colorsPicker_ColorsChanged();

private:
    Ui::CustomGradientWave *ui;

    void SetDynamicStrings();

    double progress = 0.0;
    RGBColor GetColor(float, float, float, float);

    QImage gradient;
    void GenerateGradient();

    int spread    = 100;
    int direction = HORIZONTAL;
    int height    = 50;
    int width     = 50;

    std::vector<CustomGradientWavePreset> presets =
    {
        {QT_TR_NOOP("Unicorn Vomit"), std::vector<RGBColor>{
             HEXCOLOR(0xff0000),
             HEXCOLOR(0xff00e6),
             HEXCOLOR(0x0000ff),
             HEXCOLOR(0x00b3ff),
             HEXCOLOR(0x00ff51),
             HEXCOLOR(0xeaff00),
             HEXCOLOR(0xffb300),
             HEXCOLOR(0xff0000)
         }},

        {QT_TR_NOOP("Borealis"), std::vector<RGBColor>{
             HEXCOLOR(0x14e81e),
             HEXCOLOR(0x00ea8d),
             HEXCOLOR(0x017ed5),
             HEXCOLOR(0xb53dff),
             HEXCOLOR(0x8d00c4),
             HEXCOLOR(0x14e81e),
         }},

        {QT_TR_NOOP("Ocean"), std::vector<RGBColor>{
             HEXCOLOR(0x00007f),
             HEXCOLOR(0x0000ff),
             HEXCOLOR(0x00ffff),
             HEXCOLOR(0x00aaff),
             HEXCOLOR(0x00007f),
         }},

        {QT_TR_NOOP("Pink/Blue"), std::vector<RGBColor>{
             HEXCOLOR(0xfe00c5),
             HEXCOLOR(0x00c5ff),
             HEXCOLOR(0x00c5ff),
             HEXCOLOR(0xfe00c5)
         }},

        {QT_TR_NOOP("Pink/Gold"), std::vector<RGBColor>{
             HEXCOLOR(0xfee000),
             HEXCOLOR(0xfe00fe),
             HEXCOLOR(0xfe00fe),
             HEXCOLOR(0xfee000),
         }},

        {QT_TR_NOOP("Pulse"), std::vector<RGBColor>{
             HEXCOLOR(0xff5500),
             HEXCOLOR(0x000000),
             HEXCOLOR(0x000000),
             HEXCOLOR(0x000000),
             HEXCOLOR(0xff5500),
         }},

        {QT_TR_NOOP("Purple/Orange"), std::vector<RGBColor>{
             HEXCOLOR(0xff2100),
             HEXCOLOR(0xaa00ff),
             HEXCOLOR(0xaa00ff),
             HEXCOLOR(0xff2100),
             HEXCOLOR(0xff2100),
             HEXCOLOR(0xff2100)
         }},

        {QT_TR_NOOP("LightBlue/Purple"), std::vector<RGBColor>{
             HEXCOLOR(0x03fffa),
             HEXCOLOR(0x55007f),
             HEXCOLOR(0x55007f),
             HEXCOLOR(0x03fffa)
         }},

        {QT_TR_NOOP("Police Beacon"), std::vector<RGBColor>{
             HEXCOLOR(0xff0000),
             HEXCOLOR(0x0000ff),
             HEXCOLOR(0x0000ff),
             HEXCOLOR(0xff0000),
             HEXCOLOR(0xff0000)
         }},

        {QT_TR_NOOP("Seabed"), std::vector<RGBColor>{
             HEXCOLOR(0x00ff00),
             HEXCOLOR(0x0032ff),
             HEXCOLOR(0x0032ff),
             HEXCOLOR(0x00ff00),
             HEXCOLOR(0x00ff00)
         }},

        {QT_TR_NOOP("Sunset"), std::vector<RGBColor>{
             HEXCOLOR(0xff2100),
             HEXCOLOR(0xab006d),
             HEXCOLOR(0xc01c52),
             HEXCOLOR(0xd53737),
             HEXCOLOR(0xea531b),
             HEXCOLOR(0xff6e00),
             HEXCOLOR(0xff0000),
             HEXCOLOR(0xff2100)
         }},

        {QT_TR_NOOP("Vaporwave"), std::vector<RGBColor>{
             HEXCOLOR(0xFF71CE),
             HEXCOLOR(0xB967FF),
             HEXCOLOR(0x01CDFE),
             HEXCOLOR(0x05FFA1),
             HEXCOLOR(0xFFFB96),
             HEXCOLOR(0xFF71CE)
         }}
    };
};
