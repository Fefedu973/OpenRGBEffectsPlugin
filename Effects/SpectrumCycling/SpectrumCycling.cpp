/*---------------------------------------------------------*\
| SpectrumCycling.cpp                                       |
|                                                           |
|   OpenRGB Effects Plugin Spectrum Cycling Effect          |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "SpectrumCycling.h"
#include "hsv.h"
#include "ui_SpectrumCycling.h"

REGISTER_EFFECT(SpectrumCycling);

SpectrumCycling::SpectrumCycling(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::SpectrumCycling)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.MaxSpeed          = 100;
    EffectDetails.MinSpeed          = 1;
    EffectDetails.HasCustomSettings = true;
    EffectDetails.SupportsRandom    = false;

    SetSpeed(50);
}

SpectrumCycling::~SpectrumCycling()
{
    delete ui;
}

void SpectrumCycling::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void SpectrumCycling::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("Goes through every solid color of the rainbow").toStdString();
}

void SpectrumCycling::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    hsv_t HSVVal;

    HSVVal.value = 255;
    HSVVal.saturation = saturation;
    HSVVal.hue = int(progress) % 360;

    RGBColor color = RGBColor(hsv2rgb(&HSVVal));

    for(ControllerZone* controller_zone: controller_zones)
    {
        controller_zone->SetAllZoneLEDs(color, Brightness, Temperature, Tint);
    }

    progress += (float) Speed / (float) FPS;
}

void SpectrumCycling::on_saturation_valueChanged(int value)
{
    saturation = value;
}

json SpectrumCycling::SaveCustomSettings()
{
    json settings;

    settings["saturation"] = saturation;

    return settings;
}

void SpectrumCycling::LoadCustomSettings(json Settings)
{
    if (Settings.contains("saturation"))
        ui->saturation->setValue(Settings["saturation"]);
}
