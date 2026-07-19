/*---------------------------------------------------------*\
| Mask.h                                                    |
|                                                           |
|   OpenRGB Effects Plugin Mask Effect                      |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_Mask.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

namespace Ui {
class Mask;
}

class Mask : public RGBEffect
{
    Q_OBJECT

public:
    explicit Mask(QWidget *parent = nullptr);
    ~Mask();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SPECIAL, [](){return new Mask;});

    static std::string const ClassName() {return "Mask";}
    static std::string const UI_Name() { return QT_TR_NOOP("Mask"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_x_valueChanged(double);
    void on_y_valueChanged(double);
    void on_w_valueChanged(double);
    void on_h_valueChanged(double);

    void on_invert_colors_stateChanged(int);

private:
    Ui::Mask *ui;

    void SetDynamicStrings();

    RGBColor GetColor(float, float, float, float);
    QRectF restrict = QRectF(0.f, 0.f, 1.f, 1.f);

    bool invert_colors = false;
};
