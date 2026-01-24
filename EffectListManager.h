/*---------------------------------------------------------*\
| EffectListManager.h                                       |
|                                                           |
|   Single-instance class to manage registration of effects |
|   and effects list                                        |
|                                                           |
|   Adam Honse (CalcProgrammer1)                10 Aug 2024 |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <functional>
#include <string>
#include "RGBEffect.h"
#include "EffectsName.h"

class EffectListManager
{
public:
    static EffectListManager * get();

    EffectListManager();
    ~EffectListManager();

    std::map<std::string, std::vector<effect_names>>    GetCategorizedEffects();
    std::function<RGBEffect*()>                         GetEffectConstructor(std::string name);
    std::size_t                                         GetEffectsListSize();

    void                                                RegisterEffect(std::string classname,
                                                                       std::string ui_name,
                                                                       std::string category,
                                                                       std::function<RGBEffect*()> constructor);

private:
    static EffectListManager*                           instance;

    std::map<std::string, std::function<RGBEffect*()>>  effects_constructors;
    std::map<std::string, std::vector<effect_names>>    categorized_effects;
};
