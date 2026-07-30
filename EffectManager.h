/*---------------------------------------------------------*\
| EffectManager.h                                           |
|                                                           |
|   OpenRGB Effects Plugin Effect Manager                   |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <mutex>
#include <thread>
#include "ControllerZone.h"
#include "RGBEffect.h"

/*-----------------------------------------------------------------------------------------------*\
| Define so we have a reasonable variable name instead of having to use auto or the full vaiable  |
\*-----------------------------------------------------------------------------------------------*/
typedef std::chrono::time_point<std::chrono::steady_clock,std::chrono::duration<long long, std::ratio<1,10000000000>>> TCount;

class EffectManager
{
public:    
    static EffectManager* Get();

    void SetEffectActive(RGBEffect*);
    void SetEffectUnActive(RGBEffect*);
    bool IsActive(RGBEffect*);
    void RemoveMapping(RGBEffect*);

    void ClearAssignments();
    void RemapAssignedZones(const std::vector<ControllerZone*>&);
    void Assign(std::vector<ControllerZone*>, RGBEffect*);
    void SetUnresolvedZones(RGBEffect*, const std::vector<nlohmann::json>&);
    std::vector<nlohmann::json> GetUnresolvedZones(RGBEffect*);
    std::vector<ControllerZone*> GetAssignedZones(RGBEffect*);
    std::map<RGBEffect*, std::vector<ControllerZone*>> GetEffectsMapping();

    bool HasActiveEffects();

    void AddPreview(RGBEffect*, ControllerZone*);
    void RemovePreview(RGBEffect*);


private:
    EffectManager();
    ~EffectManager() {};
    void EffectThreadFunction(RGBEffect*);

    void NotifySelectionChanged(RGBEffect*);

    static EffectManager*   instance;
    std::vector<RGBEffect*> ActiveEffects;   
    std::map<RGBEffect*,std::thread*> EffectThreads;
    std::chrono::steady_clock* clock;

    std::map<RGBEffect*, std::vector<ControllerZone*>> effect_zones;

    /*-----------------------------------------------------*\
    | Assigned zones with no live ControllerZone, kept      |
    | as descriptors to resolve when the device shows       |
    \*-----------------------------------------------------*/
    std::map<RGBEffect*, std::vector<nlohmann::json>> unresolved_zones;

    std::map<RGBEffect*, ControllerZone*> previews;

    std::mutex lock;
};
