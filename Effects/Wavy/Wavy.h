/*---------------------------------------------------------*\
| Wavy.h                                                    |
|                                                           |
|   OpenRGB Effects Plugin Wavy Effect                      |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "ui_Wavy.h"
#include <QWidget>
#include "RGBEffect.h"
#include "EffectRegisterer.h"

#include <math.h>
#include <algorithm>

namespace Ui {
class Wavy;
}

class Wavy: public RGBEffect
{
    Q_OBJECT

public:
    explicit Wavy(QWidget *parent = nullptr);
    ~Wavy();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_ADVANCED, [](){return new Wavy;});

    static std::string const ClassName() {return "Wavy";}
    static std::string const UI_Name() { return QT_TR_NOOP("Wavy"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private:
    Ui::Wavy   *ui;

    void SetDynamicStrings();

    bool       Dir = true ;
    float      SineProgress      = 0.0f;
    float      WaveProgress      = 0.0f;

    float      WaveFrequency     = 1;
    float      WaveSpeed         = 1;
    float      OscillationSpeed  = 1;

    std::vector<RGBColor>   RandomColors;

    const double PI = std::atan(1)*4;

    RGBColor GetColor(int, int);
    void GenerateRandomColors();

private slots:
    void changeEvent(QEvent *event) override;
    void on_wave_freq_slider_valueChanged(int);
    void on_wave_speed_slider_valueChanged(int);
    void on_oscillation_speed_slider_valueChanged(int);
};
