/*---------------------------------------------------------*\
| OpenRGBEffectsPlugin.cpp                                  |
|                                                           |
|   OpenRGB Effects Plugin                                  |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <QMenu>
#include <QSystemTrayIcon>
#include <QThread>
#include <exception>
#include "OpenRGBEffectsPlugin.h"
#include "EffectList.h"
#include "EffectListManager.h"
#include "EffectManager.h"
#include "OpenRGBEffectSettings.h"
#include "Effects/SignalFavorites/SignalFavoriteRegistry.h"

#define SETTINGSMANAGER_UPDATE_REASON_SETTINGS_UPDATED 0

namespace
{
// SDK and profile hooks can run on host network threads. Their return values
// and load order require synchronous delivery, but GUI callers (including the
// session checkpoint) must run directly, without a nested event loop or lock.
template<typename Function>
void RunOnEffectUiThread(OpenRGBEffectTab* target, Function function)
{
    if(QThread::currentThread() == target->thread())
    {
        function();
        return;
    }

    std::exception_ptr failure;
    QMetaObject::invokeMethod(target, [&]()
    {
        // Never let an exception escape through Qt's event dispatcher. Preserve
        // the caller's normal error handling on its own thread instead.
        try { function(); }
        catch(...) { failure = std::current_exception(); }
    }, Qt::BlockingQueuedConnection);
    if(failure) std::rethrow_exception(failure);
}
}

/*---------------------------------------------------------*\
| Plugin Global Variables                                   |
\*---------------------------------------------------------*/
std::atomic<bool>               OpenRGBEffectsPlugin::controllers_updating;
std::vector<ControllerZone*>    OpenRGBEffectsPlugin::controller_zones;
std::shared_mutex               OpenRGBEffectsPlugin::controller_zones_mutex;
OpenRGBPluginAPIInterface*      OpenRGBEffectsPlugin::api = nullptr;

/*---------------------------------------------------------*\
| Plugin Information                                        |
\*---------------------------------------------------------*/
OpenRGBPluginInfo OpenRGBEffectsPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;

    info.Name               = PROJECT_NAME;
    info.Description        = PROJECT_DESC;
    info.Version            = VERSION_STRING;
    info.Commit             = GIT_COMMIT_ID;
    info.URL                = PROJECT_URL;

    info.Label              = "Effects";
    info.Location           = OPENRGB_PLUGIN_LOCATION_TOP;

    info.Icon.load(":/OpenRGBEffectsPlugin.png");

    info.ProtocolVersion    = 2;

    return(info);
}

unsigned int OpenRGBEffectsPlugin::GetPluginAPIVersion()
{
    return(OPENRGB_PLUGIN_API_VERSION);
}

/*---------------------------------------------------------*\
| Plugin Functionality                                      |
\*---------------------------------------------------------*/
void OpenRGBEffectsPlugin::Load(OpenRGBPluginAPIInterface* api_interface_ptr)
{
    /*-----------------------------------------------------*\
    | Store API interface pointer                           |
    \*-----------------------------------------------------*/
    api = api_interface_ptr;
    RegisterSignalFavoritePresets();
    
    /*-----------------------------------------------------*\
    | Log initial messages                                  |
    \*-----------------------------------------------------*/
    LOG_INFO("[OpenRGBEffectsPlugin] version %s (%s), build date %s", VERSION_STRING, GIT_COMMIT_ID, GIT_COMMIT_DATE);
    LOG_INFO("[OpenRGBEffectsPlugin] %lu effects registered", EffectListManager::get()->GetEffectsListSize());

    /*-----------------------------------------------------*\
    | Load global settings                                  |
    \*-----------------------------------------------------*/
    OpenRGBEffectSettings::LoadGlobalSettings();

    /*-----------------------------------------------------*\
    | Create the main UI widget and return it               |
    \*-----------------------------------------------------*/
    ui = new OpenRGBEffectTab();

    /*-----------------------------------------------------*\
    | Update the controller list                            |
    \*-----------------------------------------------------*/
    UpdateControllers();

    /*-----------------------------------------------------*\
    | Migrate legacy plugin profiles                        |
    \*-----------------------------------------------------*/
    MigrateLegacyProfiles();
}

QWidget* OpenRGBEffectsPlugin::GetWidget()
{
    return ui;
}

QMenu* OpenRGBEffectsPlugin::GetTrayMenu()
{
    /*-----------------------------------------------------*\
    | Create the tray menu                                  |
    \*-----------------------------------------------------*/
    QMenu* pluginsMenu = new QMenu("Effects", ui);

    pluginsMenu->setObjectName("OpenRGBEffectsPlugin::TrayMenu");

    /*-----------------------------------------------------*\
    | Add Start All Effects action                          |
    \*-----------------------------------------------------*/
    QAction* actionStartAll = new QAction("Start All Effects", ui);

    connect(actionStartAll, &QAction::triggered, [=](){
        QMetaObject::invokeMethod(ui, "StartAll", Qt::QueuedConnection);
    });

    actionStartAll->setObjectName("OpenRGBEffectsPlugin::Action::StartAll");
    actionStartAll->setProperty("OpenRGBEffectsPlugin::ActionTitle", actionStartAll->text());
    actionStartAll->setParent(pluginsMenu);

    pluginsMenu->addAction(actionStartAll);

    /*-----------------------------------------------------*\
    | Add Stop All Effects action                           |
    \*-----------------------------------------------------*/
    QAction* actionStopAll = new QAction("Stop All Effects", ui);

    connect(actionStopAll, &QAction::triggered, [=](){
        QMetaObject::invokeMethod(ui, "StopAll", Qt::QueuedConnection);
    });

    actionStopAll->setObjectName("OpenRGBEffectsPlugin::Action::StopAll");
    actionStopAll->setProperty("OpenRGBEffectsPlugin::ActionTitle", actionStopAll->text());

    pluginsMenu->addAction(actionStopAll);

    return(pluginsMenu);
}

void OpenRGBEffectsPlugin::Unload()
{
    /*-----------------------------------------------------*\
    | Log unload message                                    |
    \*-----------------------------------------------------*/
    LOG_INFO("[OpenRGBEffectsPlugin] Unloading\n");

    /*-----------------------------------------------------*\
    | Stop all effects                                      |
    \*-----------------------------------------------------*/
    RunOnEffectUiThread(ui, [this]() { ui->StopAll(); });
}

unsigned char* OpenRGBEffectsPlugin::OnSDKCommand(unsigned int pkt_id, unsigned char* data, unsigned int* data_size)
{
    unsigned char* data_out = nullptr;

    if(data_size == nullptr) return nullptr;

    switch(pkt_id)
    {
        case NET_PACKET_ID_REQUEST_EFFECT_LIST:
            // The description builder accumulates this value. Request payload
            // bytes are not part of the response and must not inflate it.
            *data_size = 0;
            RunOnEffectUiThread(ui, [&]() { data_out = ui->GetEffectListDescription(data_size); });
            break;
        case NET_PACKET_ID_START_EFFECT:
        case NET_PACKET_ID_STOP_EFFECT:
            {
                if(data == nullptr || *data_size < sizeof(unsigned short)) break;
                unsigned short name_len;
                memcpy(&name_len, data, sizeof(name_len));
                if(name_len == 0 || name_len > *data_size - sizeof(name_len)) break;
                const char* name_data = reinterpret_cast<const char*>(data + sizeof(name_len));
                // Protocol strings include exactly one trailing NUL. Validate
                // before copying so no UI call borrows network data.
                if(name_data[name_len - 1] != '\0' ||
                   memchr(name_data, '\0', name_len - 1) != nullptr) break;
                const std::string name(name_data, name_len - 1);
                const bool running = pkt_id == NET_PACKET_ID_START_EFFECT;
                RunOnEffectUiThread(ui, [this, name, running]() { ui->SetEffectState(name, running); });
            }
            break;
    }
    return data_out;
}

void OpenRGBEffectsPlugin::OnProfileAboutToLoad()
{
    RunOnEffectUiThread(ui, [this]() { ui->AboutToLoadProfile(); });
}

void OpenRGBEffectsPlugin::OnProfileLoad(nlohmann::json profile_data)
{
    RunOnEffectUiThread(ui, [this, profile_data]() { ui->LoadProfileJson(profile_data); });
}

nlohmann::json OpenRGBEffectsPlugin::OnProfileSave()
{
    nlohmann::json profile_json;

    RunOnEffectUiThread(ui, [&]() { profile_json = ui->GetProfileJson(true); });

    return(profile_json);
}

/*---------------------------------------------------------*\
| Update Signals                                            |
\*---------------------------------------------------------*/
void OpenRGBEffectsPlugin::ProfileManagerUpdated(unsigned int /*update_reason*/)
{

}

void OpenRGBEffectsPlugin::ResourceManagerUpdated(unsigned int update_reason)
{
    switch(update_reason)
    {
        case RESOURCEMANAGER_UPDATE_REASON_DEVICE_LIST_UPDATED:
            // Virtual controller registration can notify on the GUI thread.
            // Blocking delivery to that same thread is invalid and can skip
            // the remap (or deadlock on Qt versions without this detection);
            // cross-thread callers still wait for the controller remap before
            // the old controller descriptions may be released.
            QMetaObject::invokeMethod(this, "UpdateControllers",
                QThread::currentThread() == thread()
                    ? Qt::DirectConnection : Qt::BlockingQueuedConnection);
            break;
    }
}

void OpenRGBEffectsPlugin::SettingsManagerUpdated(unsigned int update_reason)
{
    switch(update_reason)
    {
        case SETTINGSMANAGER_UPDATE_REASON_SETTINGS_UPDATED:
            // The host holds its callback-list mutex here. Do not wait for the
            // GUI, which may be unregistering callbacks during shutdown. The
            // QObject context also cancels this post if the widget is deleted.
            QMetaObject::invokeMethod(ui, [target = ui]() { target->SetLanguage(); }, Qt::QueuedConnection);
            break;
    }
}

void OpenRGBEffectsPlugin::OpenRGBEffectsPluginRGBControllerCallback(void * this_ptr, unsigned int update_reason, void * /*controller_ptr*/)
{
    OpenRGBEffectsPlugin * this_obj = (OpenRGBEffectsPlugin *)this_ptr;

    switch(update_reason)
    {
        case RGBCONTROLLER_UPDATE_REASON_HIDDEN:
        case RGBCONTROLLER_UPDATE_REASON_UNHIDDEN:
            /*---------------------------------------------*\
            | This runs inside the controller's             |
            | SignalUpdate. Post the update instead of      |
            | blocking on the GUI thread: a blocking        |
            | call here deadlocks against                   |
            | UnregisterUpdateCallback/WaitSignalCalls      |
            | when a rescan is tearing callbacks down.      |
            \*---------------------------------------------*/
            QMetaObject::invokeMethod(this_obj, "UpdateControllers", Qt::QueuedConnection );
            break;

        case RGBCONTROLLER_UPDATE_REASON_DEVICE_CHANGED:
            /*---------------------------------------------*\
            | The controller's description was replaced in  |
            | place, so zone lists and unresolved           |
            | assignments must be re-resolved against it.   |
            | Post instead of blocking: this runs inside    |
            | the controller's SignalUpdate and a blocking  |
            | call deadlocks against the callback teardown  |
            | drain on a rescan.                            |
            \*---------------------------------------------*/
            QMetaObject::invokeMethod(this_obj, "UpdateControllers", Qt::QueuedConnection );
            break;
    }
}

void OpenRGBEffectsPlugin::MigrateLegacyProfiles()
{
    std::vector<std::string> profile_list = OpenRGBEffectsPlugin::api->GetProfileList();

    /*-----------------------------------------------------*\
    | Look at each file in the legacy effects profiles      |
    | directory                                             |
    \*-----------------------------------------------------*/
    try
    {
        for(const filesystem::directory_entry &entry : filesystem::directory_iterator(OpenRGBEffectSettings::ProfilesFolder()))
        {
            if(entry.path().extension() != ".bak")
            {
                bool            found        = false;
                nlohmann::json  profile_json = OpenRGBEffectSettings::load_json_file(entry.path());
                std::string     profile_name = entry.path().filename().string();

                for(std::size_t profile_idx = 0; profile_idx < profile_list.size(); profile_idx++)
                {
                    if(profile_name == profile_list[profile_idx])
                    {
                        found = true;
                        break;
                    }
                }

                if(!found)
                {
                    OpenRGBEffectsPlugin::api->SaveProfileFromPlugin(profile_name, GetPluginInfo().Name, profile_json);
                }

                /*-----------------------------------------*\
                | Rename legacy profile file with the .bak  |
                | extension so it does not get re-migrated  |
                | on subsequent startups                    |
                \*-----------------------------------------*/
                filesystem::path profile_bak_filename = entry.path();
                profile_bak_filename.concat(".bak");

                std::error_code rename_ec;
                filesystem::rename(entry.path(), profile_bak_filename, rename_ec);
            }
        }
    }
    catch(const std::exception& e)
    {
        LOG_WARNING("[OpenRGBEffectsPlugin] Exception during profile migration: %s", e.what());
    }
}

/*---------------------------------------------------------*\
| Controller List Management                                |
\*---------------------------------------------------------*/
void OpenRGBEffectsPlugin::UpdateControllers()
{
    controllers_updating = true;

    /*-----------------------------------------------------*\
    | Lock the controller zones mutex                       |
    \*-----------------------------------------------------*/
    controller_zones_mutex.lock();

    /*-----------------------------------------------------*\
    | Clear the existing controller zones                   |
    \*-----------------------------------------------------*/
    controller_zones.clear();

    /*-----------------------------------------------------*\
    | Create ControllerZones for new controllers            |
    \*-----------------------------------------------------*/
    for(RGBControllerInterface* controller : api->GetRGBControllers())
    {
        /*-------------------------------------------------*\
        | Unregister any existing callback registered to    |
        | this controller                                   |
        \*-------------------------------------------------*/
        controller->UnregisterUpdateCallback(this);

        /*-------------------------------------------------*\
        | Check if controller supports Direct mode          |
        \*-------------------------------------------------*/
        bool has_direct = false;

        for(unsigned int i = 0; i < controller->GetModeCount(); i++)
        {
            if(controller->GetModeName(i) == "Direct")
            {
                has_direct = true;
                break;
            }
        }

        /*-------------------------------------------------*\
        | Skip this controller if it doesn't have Direct    |
        | mode and the hide unsupported devices setting is  |
        | set, or if the controller indicates that it is    |
        | hidden                                            |
        \*-------------------------------------------------*/
        if((OpenRGBEffectSettings::globalSettings.hide_unsupported && !has_direct) || controller->GetHidden())
        {
            continue;
        }

        /*-------------------------------------------------*\
        | Register callback with the controller             |
        \*-------------------------------------------------*/
        controller->RegisterUpdateCallback(OpenRGBEffectsPluginRGBControllerCallback, this);

        /*-------------------------------------------------*\
        | Create a ControllerZone for each zone and each    |
        | segment in the controller                         |
        \*-------------------------------------------------*/
        for(std::size_t zone_idx = 0; zone_idx < controller->GetZoneCount(); zone_idx++)
        {
            if((controller->GetZoneSegmentCount(zone_idx) != 0) && (controller->GetZoneType(zone_idx) == ZONE_TYPE_SEGMENTED))
            {
                for(std::size_t segment_idx = 0; segment_idx < controller->GetZoneSegmentCount(zone_idx); segment_idx++)
                {
                    ControllerZone* controller_zone = new ControllerZone(controller, zone_idx, false, 100, has_direct, true, segment_idx);
                    controller_zones.push_back(controller_zone);
                }
            }
            else
            {
                ControllerZone* controller_zone = new ControllerZone(controller, zone_idx, false, 100, has_direct, false);
                controller_zones.push_back(controller_zone);
            }
        }
    }

    /*-----------------------------------------------------*\
    | Update the device list UI                             |
    \*-----------------------------------------------------*/
    ui->UpdateDeviceList();

    /*-----------------------------------------------------*\
    | Remap effect assignments onto the new zones. Must     |
    | complete before this function returns; the old        |
    | controllers are deleted once the update callback      |
    | returns                                               |
    \*-----------------------------------------------------*/
    EffectManager::Get()->RemapAssignedZones(controller_zones);

    /*-----------------------------------------------------*\
    | Sync the device list selection for the visible        |
    | effect                                                |
    \*-----------------------------------------------------*/
    ui->SyncSelection();

    /*-----------------------------------------------------*\
    | Unlock the controller zones mutex                     |
    \*-----------------------------------------------------*/
    controller_zones_mutex.unlock();

    controllers_updating = false;
}
