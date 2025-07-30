/*---------------------------------------------------------*\
| EffectManager.cpp                                         |
|                                                           |
|   OpenRGB Effects Plugin Effect Manager                   |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <set>
#include "EffectManager.h"
#include "OpenRGBEffectsPlugin.h"

EffectManager* EffectManager::instance;

EffectManager::EffectManager()
{
    clock = new std::chrono::steady_clock();
}

EffectManager* EffectManager::Get()
{
    if(!instance)
    {
        instance = new EffectManager();
    }

    return(instance);
}

void EffectManager::SetEffectActive(RGBEffect* Effect)
{    
    Effect->EffectState(true);

    if (EffectThreads.find(Effect) == EffectThreads.end())
    {
        ActiveEffects.push_back(Effect);
        EffectThreads[Effect] = nullptr;
        EffectThreads[Effect] = new std::thread(&EffectManager::EffectThreadFunction,this, Effect);
    }
}

void EffectManager::SetEffectUnActive(RGBEffect* Effect)
{
    Effect->EffectState(false);

    if (EffectThreads.find(Effect) != EffectThreads.end())
    {
        std::thread* thread = EffectThreads[Effect];
        EffectThreads.erase(Effect);
        thread->join();
        delete thread;

        std::vector<RGBEffect*>::iterator position = std::find(ActiveEffects.begin(), ActiveEffects.end(), Effect);
        ActiveEffects.erase(position);
    }
}

bool EffectManager::IsActive(RGBEffect* effect)
{
    return(EffectThreads.find(effect) != EffectThreads.end());
}

void EffectManager::RemoveMapping(RGBEffect* effect)
{
    effect_zones.erase(effect);
    previews.erase(effect);
}

void EffectManager::ClearAssignments()
{
    effect_zones.clear();
    previews.clear();
}

void EffectManager::Assign(std::vector<ControllerZone*> controller_zones, RGBEffect* effect)
{
    LOG_VERBOSE("[OpenRGBEffectsPlugin] Assigning %lu zones to %s", controller_zones.size(), effect->EffectDetails.EffectName.c_str());

    /*-----------------------------------------------------*\
    | Lock                                                  |
    \*-----------------------------------------------------*/
    lock.lock();

    /*-----------------------------------------------------*\
    | Update selected effect's zones list                   |
    \*-----------------------------------------------------*/
    effect_zones[effect] = controller_zones;

    /*-----------------------------------------------------*\
    | Remove zones from other effects                       |
    \*-----------------------------------------------------*/
    std::map<RGBEffect*, std::vector<ControllerZone*>>::iterator effect_zones_iterator;

    for(effect_zones_iterator = effect_zones.begin(); effect_zones_iterator != effect_zones.end(); effect_zones_iterator++)
    {
        RGBEffect*                      other_effect    = effect_zones_iterator->first;

        if(other_effect == effect)
        {
            continue;
        }

        std::vector<ControllerZone*>    remaining_zones;
        std::vector<ControllerZone*>    current_zones   = effect_zones_iterator->second;

        for(ControllerZone* zone : current_zones)
        {
            if(std::find(controller_zones.begin(), controller_zones.end(), zone) == controller_zones.end())
            {
                remaining_zones.push_back(zone);
            }
        }

        effect_zones[other_effect] = remaining_zones;
    }

    /*-----------------------------------------------------*\
    | Put controllers into Direct mode                      |
    \*-----------------------------------------------------*/
    std::set<RGBControllerInterface*> controllers;

    for(ControllerZone* controller_zone: controller_zones)
    {
        controllers.insert(controller_zone->controller);
    }

    for(RGBControllerInterface* controller : controllers)
    {
        for(unsigned int i = 0 ; i < controller->GetModeCount(); i++)
        {
            if(controller->GetModeName(i) == "Direct")
            {
                controller->SetActiveMode(i);
                break;
            }
        }
    }

    NotifySelectionChanged(effect);

    /*-----------------------------------------------------*\
    | Unlock                                                |
    \*-----------------------------------------------------*/
    lock.unlock();
}

std::vector<ControllerZone*> EffectManager::GetAssignedZones(RGBEffect* effect)
{
    return(effect_zones[effect]);
}

std::map<RGBEffect*, std::vector<ControllerZone*>>EffectManager::GetEffectsMapping()
{
    return(effect_zones);
}

void EffectManager::EffectThreadFunction(RGBEffect* effect)
{
    LOG_VERBOSE("[OpenRGBEffectsPlugin] Effect %s thread started", effect->EffectDetails.EffectName.c_str());

    TCount  effect_start        = clock->now();
    int     last_total_duration = -1;

    while(EffectThreads.find(effect) != EffectThreads.end())
    {
        TCount start = clock->now();

        if(!OpenRGBEffectsPlugin::controllers_updating)
        {
            /*---------------------------------------------*\
            | Lock                                          |
            \*---------------------------------------------*/
            lock.lock();
            OpenRGBEffectsPlugin::controller_zones_mutex.lock_shared();

            /*---------------------------------------------*\
            | Create a list of zones used by this effect    |
            \*---------------------------------------------*/
            std::vector<ControllerZone*>                    controller_zones    = effect_zones[effect];

            /*---------------------------------------------*\
            | Step the effect, which updates the controller |
            | internal states                               |
            \*---------------------------------------------*/
            effect->StepEffect(controller_zones);

            /*---------------------------------------------*\
            | Update the LEDs for all selected zones, which |
            | writes the updates to hardware                |
            \*---------------------------------------------*/
            std::set<RGBControllerInterface*> controllers;

            for(ControllerZone* controller_zone: controller_zones)
            {
                controllers.insert(controller_zone->controller);
            }

            for(RGBControllerInterface* controller : controllers)
            {
                controller->UpdateLEDs();
            }

            /*---------------------------------------------*\
            | If there is a preview enabled, update it      |
            \*---------------------------------------------*/
            std::map<RGBEffect*, ControllerZone*>::iterator preview_iterator    = previews.find(effect);

            if(preview_iterator != previews.end())
            {
                ControllerZone*                 preview_zone                    = preview_iterator->second;
                std::vector<ControllerZone*>    preview_zones;

                preview_zones.push_back(preview_zone);

                effect->StepEffect(preview_zones);

                preview_zone->controller->UpdateLEDs();
            }

            /*---------------------------------------------*\
            | Unlock                                        |
            \*---------------------------------------------*/
            OpenRGBEffectsPlugin::controller_zones_mutex.unlock_shared();
            lock.unlock();

            /*---------------------------------------------*\
            | Compute FPS and duration                      |
            \*---------------------------------------------*/
            TCount  end             = clock->now();
            int     FPS             = effect->GetFPS();
            int     FPSDelay        = 1000000 / (float)FPS;
            int     duration        = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            int     delta           = FPSDelay - duration;
            int     total_duration  = std::chrono::duration_cast<std::chrono::seconds>(end - effect_start).count();

            /*---------------------------------------------*\
            | Emit measurement in ms every second           |
            \*---------------------------------------------*/
            if(total_duration > last_total_duration)
            {
                last_total_duration = total_duration;
                effect->EmitMeasure(duration * 0.001, total_duration);
            }

            if(delta > 0)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(delta));
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    LOG_VERBOSE("[OpenRGBEffectsPlugin] Effect %s thread ended", effect->EffectDetails.EffectName.c_str());
}

bool EffectManager::HasActiveEffects()
{
    return(!ActiveEffects.empty());
}

void EffectManager::AddPreview(RGBEffect* effect, ControllerZone* preview)
{
    lock.lock();

    previews[effect] = preview;    
    NotifySelectionChanged(effect);

    lock.unlock();
}

void EffectManager::RemovePreview(RGBEffect* effect)
{
    lock.lock();

    previews.erase(effect);    
    NotifySelectionChanged(effect);

    lock.unlock();
}

void EffectManager::NotifySelectionChanged(RGBEffect* effect)
{
    /*-----------------------------------------------------*\
    | Notify effect zones has changed                       |
    \*-----------------------------------------------------*/
    std::vector<ControllerZone*> new_zones =  effect_zones[effect];

    if (previews.find(effect) != previews.end())
    {
        new_zones.push_back(previews[effect]);
    }

    effect->OnControllerZonesListChanged(new_zones);
}
