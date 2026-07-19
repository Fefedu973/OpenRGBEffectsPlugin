/*---------------------------------------------------------*\
| Sunrise.h                                                 |
|                                                           |
|   OpenRGB Effects Plugin Sunrise Effect                   |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_Sunrise.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

namespace Ui {
class Sunrise;
}

class Sunrise : public RGBEffect
{
    Q_OBJECT

public:
    explicit Sunrise(QWidget *parent = nullptr);
    ~Sunrise();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_ADVANCED, [](){return new Sunrise;});

    static std::string const ClassName() {return "Sunrise";}
    static std::string const UI_Name() { return QT_TR_NOOP("Sunrise"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_max_intensity_valueChanged(int);
    void on_intensity_speed_valueChanged(int);
    void on_radius_valueChanged(int);
    void on_grow_speed_valueChanged(int);
    void on_motion_stateChanged(int);
    void on_run_once_stateChanged(int);

private:
    Ui::Sunrise *ui;

    void SetDynamicStrings();

    double time = 0;
    double progress = 0;
    double y_shift = -1;

    RGBColor GetColor(float, float, float, float);
    QImage image = QImage(100, 1, QImage::Format_RGB32);

    int max_intensity = 80; // 1 - 100
    int intensity_speed = 10; // 1 - 100
    bool motion = false; // true - false
    int radius = 50; // 1 - 100
    int grow_speed = 10; // 1 - 20
    bool run_once = false;

};
