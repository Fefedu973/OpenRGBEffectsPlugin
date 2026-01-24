/*---------------------------------------------------------*\
| OpenRGBEffectsPlugin.h                                    |
|                                                           |
|   OpenRGB Effects Plugin                                  |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QObject>
#include <QWidget>
#include "OpenRGBEffectTab.h"
#include "OpenRGBPluginInterface.h"
#include "ResourceManagerInterface.h"

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
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID)
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    ~OpenRGBEffectsPlugin() {};

    /*-------------------------------------------------------------------------------------------------*\
    | Plugin Information                                                                                |
    \*-------------------------------------------------------------------------------------------------*/
    virtual OpenRGBPluginInfo   GetPluginInfo()                                                                 override;
    virtual unsigned int        GetPluginAPIVersion()                                                           override;

    /*-------------------------------------------------------------------------------------------------*\
    | Plugin Functionality                                                                              |
    \*-------------------------------------------------------------------------------------------------*/
    virtual void                Load(ResourceManagerInterface* resource_manager_ptr)                            override;
    virtual QWidget*            GetWidget()                                                                     override;
    virtual QMenu*              GetTrayMenu()                                                                   override;
    virtual void                Unload()                                                                        override;
    static unsigned char*       HandleSDK(void * instance, unsigned int pkt_id, unsigned char* data, unsigned int* data_size);

    /*-------------------------------------------------------------------------------------------------*\
    | Plugin Variables                                                                                  |
    \*-------------------------------------------------------------------------------------------------*/
    static ResourceManagerInterface*     RMPointer;

private:
    static void                 DeviceListChangedCallback(void* ptr);
    OpenRGBEffectTab*           ui;
};
