/*---------------------------------------------------------*\
| Clock.h                                                   |
|                                                           |
|   OpenRGB Effects Plugin Clock Effect                     |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_Clock.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

enum
{
    CLOCK_MODE_12_HOUR = 0,
    CLOCK_MODE_24_HOUR = 1
};

namespace Ui {
class Clock;
}

class Clock : public RGBEffect
{
    Q_OBJECT

public:
    explicit Clock(QWidget *parent = nullptr);
    ~Clock();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SIMPLE, [](){return new Clock;});

    static std::string const ClassName() {return "Clock";}
    static std::string const UI_Name() { return QT_TR_NOOP("Clock"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_clock_mode_combo_box_currentIndexChanged(int);

private:    
    Ui::Clock *ui;

    void SetDynamicStrings();

    int clock_mode = CLOCK_MODE_12_HOUR;

    float h = 0.f;
    float m = 0.f;
    float s = 0.f;

    RGBColor GetColor(float, float, float);
};
