/*---------------------------------------------------------*\
| BreathingCircle.h                                         |
|                                                           |
|   OpenRGB Effects Plugin Breathing Circle Effect          |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_BreathingCircle.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

namespace Ui {
class BreathingCircle;
}

class BreathingCircle : public RGBEffect
{
    Q_OBJECT

public:
    explicit BreathingCircle(QWidget *parent = nullptr);
    ~BreathingCircle();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SIMPLE, [](){return new BreathingCircle;});

    static std::string const ClassName() {return "BreathingCircle";}
    static std::string const UI_Name() { return QT_TR_NOOP("Breathing Circle"); }

    void StepEffect(std::vector<ControllerZone*>) override;

private:
    Ui::BreathingCircle *ui;

    void SetDynamicStrings();

    double time = 0.0;
    double progress = 0.0;
    bool growing = true;
    RGBColor randomColor = ColorUtils::RandomRGBColor();
    RGBColor GetColor(float, float, float, float);

private slots:
    void changeEvent(QEvent *event) override;
};
