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
    lock.lock();

    effect_zones.erase(effect);
    unresolved_zones.erase(effect);
    previews.erase(effect);

    lock.unlock();
}

void EffectManager::ClearAssignments()
{
    lock.lock();

    effect_zones.clear();
    unresolved_zones.clear();
    previews.clear();

    lock.unlock();
}

/*---------------------------------------------------------*\
| First unclaimed zone matching the descriptor. Claiming    |
| keeps identical devices as distinct assignments.          |
\*---------------------------------------------------------*/
static ControllerZone* ResolveZone
    (
    const nlohmann::json&                   zone_json,
    const std::vector<ControllerZone*>&     new_zones,
    std::set<ControllerZone*>&              claimed
    )
{
    for(ControllerZone* new_zone : new_zones)
    {
        if(claimed.find(new_zone) == claimed.end() && new_zone->matches_json(zone_json))
        {
            new_zone->reverse           = zone_json.contains("reverse")         ? (bool)zone_json["reverse"]                  : false;
            new_zone->self_brightness   = zone_json.contains("self_brightness") ? (unsigned int)zone_json["self_brightness"]  : 100;

            claimed.insert(new_zone);

            return(new_zone);
        }
    }

    return(nullptr);
}

void EffectManager::RemapAssignedZones(const std::vector<ControllerZone*>& new_zones)
{
    /*-----------------------------------------------------*\
    | Lock                                                  |
    \*-----------------------------------------------------*/
    lock.lock();

    std::set<ControllerZone*> claimed;

    /*-----------------------------------------------------*\
    | Rebuild each effect's assignment against the new zone |
    | list. Zones that don't resolve are kept as            |
    | descriptors so they can bind on a later update.       |
    \*-----------------------------------------------------*/
    std::map<RGBEffect*, std::vector<ControllerZone*>>::iterator effect_zones_iterator;

    for(effect_zones_iterator = effect_zones.begin(); effect_zones_iterator != effect_zones.end(); effect_zones_iterator++)
    {
        RGBEffect*                      effect              = effect_zones_iterator->first;

        std::vector<ControllerZone*>    remapped_zones;
        std::vector<nlohmann::json>     still_unresolved;

        /*-------------------------------------------------*\
        | Old controllers are still alive here, so their    |
        | descriptors can be read for matching              |
        \*-------------------------------------------------*/
        for(ControllerZone* old_zone : effect_zones_iterator->second)
        {
            /*---------------------------------------------*\
            | Zone is already in the new list               |
            \*---------------------------------------------*/
            if(std::find(new_zones.begin(), new_zones.end(), old_zone) != new_zones.end())
            {
                if(claimed.find(old_zone) == claimed.end())
                {
                    claimed.insert(old_zone);
                    remapped_zones.push_back(old_zone);
                }

                continue;
            }

            nlohmann::json  zone_json   = old_zone->to_json();
            ControllerZone* new_zone    = ResolveZone(zone_json, new_zones, claimed);

            if(new_zone != nullptr)
            {
                remapped_zones.push_back(new_zone);
            }
            else
            {
                still_unresolved.push_back(zone_json);
            }
        }

        /*-------------------------------------------------*\
        | Retry descriptors that had no live zone           |
        \*-------------------------------------------------*/
        std::map<RGBEffect*, std::vector<nlohmann::json>>::iterator unresolved_iterator = unresolved_zones.find(effect);

        if(unresolved_iterator != unresolved_zones.end())
        {
            for(const nlohmann::json& zone_json : unresolved_iterator->second)
            {
                ControllerZone* new_zone = ResolveZone(zone_json, new_zones, claimed);

                if(new_zone != nullptr)
                {
                    LOG_VERBOSE("[OpenRGBEffectsPlugin] Zone %s is now available, assigning it to %s",
                                new_zone->display_name().c_str(), effect->EffectDetails.EffectName.c_str());

                    remapped_zones.push_back(new_zone);
                }
                else
                {
                    still_unresolved.push_back(zone_json);
                }
            }
        }

        effect_zones_iterator->second = remapped_zones;

        if(still_unresolved.empty())
        {
            unresolved_zones.erase(effect);
        }
        else
        {
            unresolved_zones[effect] = still_unresolved;
        }

        NotifySelectionChanged(effect);
    }

    lock.unlock();
}

void EffectManager::SetUnresolvedZones(RGBEffect* effect, const std::vector<nlohmann::json>& zones)
{
    lock.lock();

    if(zones.empty())
    {
        unresolved_zones.erase(effect);
    }
    else
    {
        unresolved_zones[effect] = zones;

        /*-------------------------------------------------*\
        | RemapAssignedZones iterates effect_zones, so keep |
        | an entry even when nothing resolved               |
        \*-------------------------------------------------*/
        effect_zones.emplace(effect, std::vector<ControllerZone*>());
    }

    lock.unlock();
}

std::vector<nlohmann::json> EffectManager::GetUnresolvedZones(RGBEffect* effect)
{
    std::vector<nlohmann::json> zones;

    lock.lock();

    std::map<RGBEffect*, std::vector<nlohmann::json>>::iterator unresolved_iterator = unresolved_zones.find(effect);

    if(unresolved_iterator != unresolved_zones.end())
    {
        zones = unresolved_iterator->second;
    }

    lock.unlock();

    return(zones);
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
    lock.lock();

    std::vector<ControllerZone*> zones = effect_zones[effect];

    lock.unlock();

    return(zones);
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
            | Lock. controller_zones_mutex is taken before  |
            | lock everywhere both are held                 |
            \*---------------------------------------------*/
            OpenRGBEffectsPlugin::controller_zones_mutex.lock_shared();
            lock.lock();

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
            lock.unlock();
            OpenRGBEffectsPlugin::controller_zones_mutex.unlock_shared();

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
