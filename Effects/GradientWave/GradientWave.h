/*---------------------------------------------------------*\
| GradientWave.h                                            |
|                                                           |
|   OpenRGB Effects Plugin Gradient Wave Effect             |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "RGBEffect.h"
#include "EffectRegisterer.h"

class GradientWave: public RGBEffect
{
public:
    GradientWave();
    ~GradientWave(){}

    EFFECT_REGISTERER(ClassName(), ClassName(), CAT_ADVANCED, [](){return new GradientWave;});

    static std::string const ClassName() { return "GradientWave"; }

    void StepEffect(std::vector<ControllerZone*>) override;
    void SetRandomColorsEnabled(bool) override;
    void OnControllerZonesListChanged(std::vector<ControllerZone*>) override;

private:
    std::vector<float> Progress;
    RGBColor RandomColorList[2];
};
