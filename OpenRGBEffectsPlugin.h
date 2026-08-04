/*---------------------------------------------------------*\
| OpenRGBEffectsPlugin.h                                    |
|                                                           |
|   OpenRGB Effects Plugin                                  |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <atomic>
#include <shared_mutex>
#include <vector>
#include <QMenu>
#include <QObject>
#include <QWidget>
#include "ControllerZone.h"
#include "LogManager.h"
#include "OpenRGBEffectTab.h"
#include "OpenRGBPluginInterface.h"
#include "ResourceManagerCallback.h"
#include "RGBControllerInterface.h"

enum
{
    NET_PACKET_ID_REQUEST_EFFECT_LIST           = 0,
    NET_PACKET_ID_START_EFFECT                  = 20,
    NET_PACKET_ID_STOP_EFFECT                   = 21,
    NET_PACKET_ID_REQUEST_EFFECTS_PROFILE_LIST  = 22,
    NET_PACKET_ID_LOAD_EFFECTS_PROFILE          = 23
};

class OpenRGBEffectsPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "OpenRGBEffectsPlugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    ~OpenRGBEffectsPlugin() {};

    /*-----------------------------------------------------*\
    | Plugin Information                                    |
    \*-----------------------------------------------------*/
    virtual OpenRGBPluginInfo   GetPluginInfo()                                                                 override;
    virtual unsigned int        GetPluginAPIVersion()                                                           override;

    /*-----------------------------------------------------*\
    | Plugin Functionality                                  |
    \*-----------------------------------------------------*/
    virtual void                Load(OpenRGBPluginAPIInterface * api_interface_ptr)                             override;
    virtual QWidget*            GetWidget()                                                                     override;
    virtual QMenu*              GetTrayMenu()                                                                   override;
    virtual void                OnProfileAboutToLoad()                                                          override;
    virtual void                OnProfileLoad(nlohmann::json profile_data)                                      override;
    virtual nlohmann::json      OnProfileSave()                                                                 override;
    virtual void                Unload()                                                                        override;
    unsigned char*              OnSDKCommand(unsigned int pkt_id, unsigned char* data, unsigned int* data_size) override;

    /*-----------------------------------------------------*\
    | Update Signals                                        |
    \*-----------------------------------------------------*/
    void                        ProfileManagerUpdated(unsigned int update_reason)                               override;
    void                        ResourceManagerUpdated(unsigned int update_reason)                              override;
    void                        SettingsManagerUpdated(unsigned int update_reason)                              override;

private:
    /*-----------------------------------------------------*\
    | User interface widget                                 |
    \*-----------------------------------------------------*/
    OpenRGBEffectTab*           ui;

    void                        MigrateLegacyProfiles();
    
    /*-----------------------------------------------------*\
    | Callbacks                                             |
    \*-----------------------------------------------------*/
    static void                 OpenRGBEffectsPluginResourceManagerCallback(void* this_ptr, unsigned int update_reason);
    static void                 OpenRGBEffectsPluginRGBControllerCallback(void* this_ptr, unsigned int update_reason, void * /*controller_ptr*/);

private slots:
    /*-----------------------------------------------------*\
    | Controller List Management                            |
    \*-----------------------------------------------------*/
    void                        UpdateControllers();

public:
    /*-----------------------------------------------------*\
    | Plugin Global Variables                               |
    \*-----------------------------------------------------*/
    static std::atomic<bool>            controllers_updating;
    static std::vector<ControllerZone*> controller_zones;
    static std::shared_mutex            controller_zones_mutex;
    static OpenRGBPluginAPIInterface *  api;
};

/*---------------------------------------------------------*\
| LogManager logging macros                                 |
\*---------------------------------------------------------*/
#undef  LogAppend
#define LogAppend(level, ...)   OpenRGBEffectsPlugin::api->LogEntry(__FILE__, __LINE__, level, __VA_ARGS__)
