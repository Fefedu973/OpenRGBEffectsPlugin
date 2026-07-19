/*---------------------------------------------------------*\
| CrossingBeams.h                                           |
|                                                           |
|   OpenRGB Effects Plugin Crossing Beams Effect            |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_CrossingBeams.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "hsv.h"

namespace Ui {
class CrossingBeams;
}

class CrossingBeams : public RGBEffect
{
    Q_OBJECT

public:
    explicit CrossingBeams(QWidget *parent = nullptr);
    ~CrossingBeams();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_BEAMS, [](){return new CrossingBeams;});

    static std::string const ClassName() {return "CrossingBeams";}
    static std::string const UI_Name() { return QT_TR_NOOP("Crossing Beams"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;
    void SetUserColors(std::vector<RGBColor> colors) override;
    void SetRandomColorsEnabled(bool value) override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_v_speed_valueChanged(int);
    void on_h_speed_valueChanged(int);
    void on_glow_valueChanged(int);
    void on_thickness_valueChanged(int);

private:
    Ui::CrossingBeams *ui;

    void SetDynamicStrings();

    double progress = 0;
    double sine_x = 0;
    double sine_y = 0;

    int v_speed = 1;
    int h_speed = 1;
    int glow = 1;
    int thickness = 0;

    hsv_t hsv1;
    hsv_t hsv2;

    void ResetUserColors();
    RGBColor GetColor(unsigned int, unsigned int, unsigned int, unsigned int);

};
