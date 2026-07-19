/*---------------------------------------------------------*\
| Lightning.h                                               |
|                                                           |
|   OpenRGB Effects Plugin Lightning Effect                 |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include <stdlib.h>

#include "ui_Lightning.h"
#include "hsv.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

namespace Ui {
class Lightning;
}

class Lightning: public RGBEffect
{
    Q_OBJECT

public:
    explicit Lightning(QWidget *parent = nullptr);
    ~Lightning();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_RANDOM, [](){return new Lightning;});

    static std::string const ClassName() {return "Lightning";}
    static std::string const UI_Name() { return QT_TR_NOOP("Lightning"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void SetUserColors(std::vector<RGBColor>) override;
    void OnControllerZonesListChanged(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_lightning_mode_currentIndexChanged(int);

private:
    Ui::Lightning *ui;

    void SetDynamicStrings();

    hsv_t UserHSV;
    std::map<ControllerZone*, std::vector<hsv_t>> Lightnings;
    RGBColor TriggerLightning(ControllerZone*, int);

    enum lightning_mode_value
    {
        WHOLE_ZONE  = 0,
        PER_LED     = 1
    };

    lightning_mode_value lightning_mode = WHOLE_ZONE;
};
