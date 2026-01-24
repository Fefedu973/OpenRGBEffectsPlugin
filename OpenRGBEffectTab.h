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

    void SetEffectState(std::string name, bool running);
    void LoadProfile(std::string);
    unsigned char * GetEffectListDescription(unsigned int* data_size);
    unsigned char * GetProfileListDescription(unsigned int* data_size);
signals:
    void ProfileListUpdated();

public slots:
    void DeviceListChanged();
    void LoadProfileList();
    void LoadProfile(QString);
    void StartAll();
    void StopAll();

private slots:
    void changeEvent(QEvent *event) override;
    void on_device_list_SelectionChanged();
    void on_EffectTabs_currentChanged(int);

    void OnStopEffects();
    void SaveProfileAction();
    void DeleteProfileAction();
    void PluginInfoAction();
    void GlobalSettingsAction();

private:
    Ui::OpenRGBEffectTab *ui;
    EffectList* effect_list = nullptr;
    QMenu* load_profile_menu;

    std::string current_i18n_file = "default";
    QTranslator translator;

    std::string latest_loaded_profile = "";

    void AddGlobalMenus();
    void InitEffectTabs();
    void CreateEffectTab(RGBEffect*);
    void InitDeviceList();
    void LoadEffect(json);
    void ClearAll();
    void SetLanguage();
};
