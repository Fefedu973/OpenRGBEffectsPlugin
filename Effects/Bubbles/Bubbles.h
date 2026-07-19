/*---------------------------------------------------------*\
| Bubbles.h                                                 |
|                                                           |
|   OpenRGB Effects Plugin Bubbles Effect                   |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "ui_Bubbles.h"
#include <QWidget>
#include "RGBEffect.h"
#include "EffectRegisterer.h"

namespace Ui {
class Bubbles;
}

class Bubbles: public RGBEffect
{
    Q_OBJECT

public:
    explicit Bubbles(QWidget *parent = nullptr);
    ~Bubbles();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_ADVANCED, [](){return new Bubbles;});

    static std::string const ClassName() {return "Bubbles";}
    static std::string const UI_Name() { return QT_TR_NOOP("Bubbles"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private:
    Ui::Bubbles   *ui;

    void SetDynamicStrings();

    std::vector<double>    speeds;
    std::vector<double>    bubbles;
    std::vector<RGBColor>  colors;
    std::vector<QPointF>   centers;

    RGBColor background;

    void InitBubble();
    void Cleanup();

    RGBColor GetColor(int, int, int, int);

    unsigned int max_bubbles = 10;
    unsigned int rarity = 50;
    unsigned int speed_mult = 1;
    unsigned int max_expansion = 100;
    unsigned int bubbles_thickness = 10;

private slots:
    void changeEvent(QEvent *event) override;
    void on_max_bubbles_valueChanged(int);
    void on_rarity_valueChanged(int);
    void on_max_expansion_valueChanged(int);
    void on_bubbles_thickness_valueChanged(int);
    void on_speed_mult_valueChanged(int);
    void on_background_ColorSelected(QColor);
};
