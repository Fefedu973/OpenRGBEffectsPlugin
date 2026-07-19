/*---------------------------------------------------------*\
| RadialRainbow.h                                           |
|                                                           |
|   OpenRGB Effects Plugin Radial Rainbow Effect            |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_RadialRainbow.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

namespace Ui {
class RadialRainbow;
}

class RadialRainbow : public RGBEffect
{
    Q_OBJECT

public:
    explicit RadialRainbow(QWidget *parent = nullptr);
    ~RadialRainbow();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_RAINBOW, [](){return new RadialRainbow;});

    static std::string const ClassName() {return "RadialRainbow";}
    static std::string const UI_Name() { return QT_TR_NOOP("Radial Rainbow"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_cx_valueChanged(int);
    void on_cy_valueChanged(int);
    void on_shape_currentIndexChanged(int);

private:
    Ui::RadialRainbow *ui;

    void SetDynamicStrings();

    double progress = 0.f;
    RGBColor GetColor(unsigned int, unsigned int, double, double, bool);
    unsigned int cx_shift = 50;
    unsigned int cy_shift = 50;
    unsigned int shape = 0;
};
