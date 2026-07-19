/*---------------------------------------------------------*\
| Layers.h                                                  |
|                                                           |
|   OpenRGB Effects Plugin Layers Effect                    |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include <QTabBar>
#include "ui_Layers.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "LayerGroupEntry.h"

namespace Ui {
class Layers;
}

class Layers : public RGBEffect
{
    Q_OBJECT

public:
    explicit Layers(QWidget *parent = nullptr);
    ~Layers();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SPECIAL, [](){return new Layers;});

    static std::string const ClassName() {return "Layers";}
    static std::string const UI_Name() { return QT_TR_NOOP("Layers"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void OnControllerZonesListChanged(std::vector<ControllerZone*>) override;
    void EffectState(bool) override;

    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;


private slots:
    void changeEvent(QEvent *event) override;
    void OnAddGroupClicked();

private:
    Ui::Layers *ui;

    void SetDynamicStrings();

    std::vector<ControllerZone*> assigned_zones;
    std::vector<LayerGroupEntry*> layer_groups;

    LayerGroupEntry* AddLayerGroup();
    void ClearLayerGroups();
};
