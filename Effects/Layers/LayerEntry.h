/*---------------------------------------------------------*\
| LayerEntry.h                                              |
|                                                           |
|   OpenRGB Effects Plugin Layer Entry                      |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include <nlohmann/json.hpp>
#include "RGBEffect.h"
#include "ColorUtils.h"
#include "OpenRGBEffectPage.h"

using json = nlohmann::json;

namespace Ui {
class LayerEntry;
}

class LayerEntry : public QWidget
{
    Q_OBJECT

public:
    explicit LayerEntry(QWidget *parent = nullptr, RGBEffect* effect = nullptr);
    ~LayerEntry();

    json ToJson();
    static LayerEntry* FromJson(json);

    void StepEffect(std::vector<ControllerZone*>);
    void OnControllerZonesListChanged(std::vector<ControllerZone*>);
    void EffectState(bool);

    ColorBlendFn GetComposerFn();

signals:
    void Remove();

private slots:
    void changeEvent(QEvent *event) override;
    void on_composer_fn_currentIndexChanged(int);
    void on_remove_clicked();
    void on_edit_clicked();

private:
    Ui::LayerEntry *ui;
    void PopulateCombos();

    std::vector<ControllerZone*> assigned_zones;
    bool state = false;
    ColorBlendFn composer_fn = MULTIPLY;

    RGBEffect* effect;
    OpenRGBEffectPage*  effect_page = nullptr;
    QDialog* effect_page_dialog = nullptr;

};
