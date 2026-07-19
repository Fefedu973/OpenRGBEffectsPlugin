/*---------------------------------------------------------*\
| Visor.h                                                   |
|                                                           |
|   OpenRGB Effects Plugin Visor Effect                     |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QEvent>
#include "RGBEffect.h"
#include "EffectRegisterer.h"

class Visor: public RGBEffect
{
    Q_OBJECT

public:
    Visor();
    ~Visor() {};

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SIMPLE, [](){return new Visor;});

    static std::string const ClassName() {return "Visor";}
    static std::string const UI_Name() { return QT_TR_NOOP("Visor"); }

    void StepEffect(std::vector<ControllerZone*>) override;

private slots:
    void changeEvent(QEvent *event) override;

private:
    void SetDynamicStrings();
    float  Progress = 0.f;

    float  p = 0.;
    float  p_step =0.;
    bool   step = false;
    bool   last_step = false;
    float  width = 0.;
    float  h_width = 0.;

    RGBColor C0;
    RGBColor C1;

    RGBColor GetColor(float i, float count);
};
