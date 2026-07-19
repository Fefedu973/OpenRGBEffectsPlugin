/*---------------------------------------------------------*\
| Hypnotoad.h                                               |
|                                                           |
|   OpenRGB Effects Plugin Hypnotoad Effect                 |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_Hypnotoad.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

enum
{
    COLOR_MODE_RAINBOW  = 0,
    COLOR_MODE_CUSTOM   = 1
};

namespace Ui {
class Hypnotoad;
}

class Hypnotoad : public RGBEffect
{
    Q_OBJECT

public:
    explicit Hypnotoad(QWidget *parent = nullptr);
    ~Hypnotoad();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_RAINBOW, [](){return new Hypnotoad;});

    static std::string const ClassName() {return "Hypnotoad";}
    static std::string const UI_Name() { return QT_TR_NOOP("Hypnotoad"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_color_mode_currentIndexChanged(int);
    void on_animation_speed_valueChanged(int);
    void on_color_rotation_speed_valueChanged(int);
    void on_animation_direction_currentIndexChanged(int);
    void on_color_rotation_direction_currentIndexChanged(int);
    void on_spacing_valueChanged(int);
    void on_thickness_valueChanged(int);
    void on_cx_valueChanged(int);
    void on_cy_valueChanged(int);
    void on_colors_picker_ColorsChanged();

private:
    Ui::Hypnotoad *ui;

    void SetDynamicStrings();

    double progress = 1000.0;
    RGBColor GetColor(unsigned int, unsigned int, float, float, bool);
    void GenerateGradient();
    QImage gradient;

    unsigned int color_mode = COLOR_MODE_RAINBOW;
    float animation_speed = 10.f;
    float color_rotation_speed = 10.f;
    unsigned int animation_direction = 0;
    unsigned int color_rotation_direction = 0;
    unsigned int spacing = 1;
    unsigned int thickness = 1;
    unsigned int cx_shift = 50;
    unsigned int cy_shift = 50;

};
