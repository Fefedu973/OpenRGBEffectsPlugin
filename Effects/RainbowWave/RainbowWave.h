/*---------------------------------------------------------*\
| RainbowWave.h                                             |
|                                                           |
|   OpenRGB Effects Plugin Rainbow Wave Effect              |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QEvent>
#include "RGBEffect.h"
#include "EffectRegisterer.h"

class RainbowWave: public RGBEffect
{
    Q_OBJECT

public:
    RainbowWave();
    ~RainbowWave() {};

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_RAINBOW, [](){return new RainbowWave;});

    static std::string const ClassName() { return "RainbowWave"; }
    static std::string const UI_Name() { return QT_TR_NOOP("Rainbow Wave"); }

    void StepEffect(std::vector<ControllerZone*>) override;

private slots:
    void changeEvent(QEvent *event) override;

private:
    void SetDynamicStrings();

    float Progress = 0;
};
