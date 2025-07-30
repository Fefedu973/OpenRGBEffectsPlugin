/*---------------------------------------------------------*\
| OpenRGBEffectTab.h                                        |
|                                                           |
|   OpenRGB Effects Plugin Effect Tab                       |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QTranslator>
#include "ui_OpenRGBEffectTab.h"
#include "RGBEffect.h"
#include "EffectList.h"

namespace Ui {
class OpenRGBEffectTab;
}

class OpenRGBEffectTab : public QWidget
{
    Q_OBJECT

public:
    explicit OpenRGBEffectTab(QWidget *parent = nullptr);
    ~OpenRGBEffectTab();

    json GetProfileJson(bool save_effects_state);
    void SetEffectState(std::string name, bool running);
    void AboutToLoadProfile();
    void LoadProfileJson(json profile_json);
    unsigned char * GetEffectListDescription(unsigned int* data_size);
    void SetLanguage();
    
public slots:
    void UpdateDeviceList();
    void StartAll();
    void StopAll();

private slots:
    void on_device_list_SelectionChanged();
    void on_EffectTabs_currentChanged(int);

    void OnStopEffects();
    void PluginInfoAction();
    void GlobalSettingsAction();

private:
    Ui::OpenRGBEffectTab *ui;
    EffectList* effect_list = nullptr;

    std::string current_i18n_file = "default";
    QTranslator translator;

    void AddGlobalMenus();
    void InitEffectTabs();
    void CreateEffectTab(RGBEffect*);
    void LoadEffect(json);
    void ClearAll();
};
