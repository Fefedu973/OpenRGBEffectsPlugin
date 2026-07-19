/*---------------------------------------------------------*\
| SwirlCircles.h                                            |
|                                                           |
|   OpenRGB Effects Plugin Swirl Circles Effect             |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_SwirlCircles.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "hsv.h"

namespace Ui {
class SwirlCircles;
}

class SwirlCircles : public RGBEffect
{
    Q_OBJECT

public:
    explicit SwirlCircles(QWidget *parent = nullptr);
    ~SwirlCircles();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_ADVANCED, [](){return new SwirlCircles;});

    static std::string const ClassName() {return "SwirlCircles";}
    static std::string const UI_Name() { return QT_TR_NOOP("Swirl Circles"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void SetUserColors(std::vector<RGBColor> colors) override;
    void SetRandomColorsEnabled(bool value) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_radius_valueChanged(int);

private:
    Ui::SwirlCircles *ui;

    void SetDynamicStrings();

    double progress = 0.0;
    int radius = 0;

    void ResetUserColors();

    hsv_t hsv1;
    hsv_t hsv2;

    RGBColor GetColor(unsigned int, unsigned int,  unsigned int,  unsigned int, float, float);

};
