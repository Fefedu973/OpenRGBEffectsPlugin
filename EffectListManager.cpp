/*---------------------------------------------------------*\
| EffectListManager.cpp                                     |
|                                                           |
|   Single-instance class to manage registration of effects |
|   and effects list                                        |
|                                                           |
|   Adam Honse (CalcProgrammer1)                10 Aug 2024 |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "EffectListManager.h"

EffectListManager* EffectListManager::instance;

EffectListManager *EffectListManager::get()
{
    if(!instance)
    {
        instance = new EffectListManager();
    }

    return instance;
}

EffectListManager::EffectListManager()
{

}

EffectListManager::~EffectListManager()
{

}

std::map<std::string, std::vector<effect_names>> EffectListManager::GetCategorizedEffects()
{
    return(categorized_effects);
}

std::function<RGBEffect*()> EffectListManager::GetEffectConstructor(std::string name)
{
    return(effects_constructors[name]);
}

std::size_t EffectListManager::GetEffectsListSize()
{
    return(effects_constructors.size());
}

void EffectListManager::RegisterEffect(std::string classname,
                                       std::string ui_name,
                                       std::string category,
                                       std::function<RGBEffect*()> constructor)
{    
    effects_constructors[classname] = constructor;

    if(!categorized_effects.count(category))
    {
        std::vector<effect_names> effects;
        categorized_effects[category] = effects;
    }

    effect_names new_effect;
    new_effect.classname    = classname;
    new_effect.ui_name      = ui_name;
    categorized_effects[category].push_back(new_effect);
}
