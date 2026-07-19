/*---------------------------------------------------------*\
| DoubleRotatingRainbow.h                                   |
|                                                           |
|   OpenRGB Effects Plugin Double Rotating Rainbow Effect   |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_DoubleRotatingRainbow.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"


namespace Ui {
class DoubleRotatingRainbow;
}

class DoubleRotatingRainbow : public RGBEffect
{
    Q_OBJECT

public:
    explicit DoubleRotatingRainbow(QWidget *parent = nullptr);
    ~DoubleRotatingRainbow();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_RAINBOW, [](){return new DoubleRotatingRainbow;});

    static std::string const ClassName() {return "DoubleRotatingRainbow";}
    static std::string const UI_Name() { return QT_TR_NOOP("Double Rotating Rainbow"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_frequency_valueChanged(int);

private:
    Ui::DoubleRotatingRainbow *ui;

    void SetDynamicStrings();

    double time = 1000.0;

    double cos_time;
    double sin_time;

    double cos_reverse_time;
    double sin_reverse_time;

    double color_speed = 1.f;

    int frequency = 1;

    RGBColor GetColor(float, float, float, float, bool);
};
