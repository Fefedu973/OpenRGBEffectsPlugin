/*---------------------------------------------------------*\
| Sequence.cpp                                              |
|                                                           |
|   OpenRGB Effects Plugin Sequence Effect                  |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "Sequence.h"
#include "ColorUtils.h"

REGISTER_EFFECT(Sequence);

Sequence::Sequence(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::Sequence)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.MaxSpeed          = 20;
    EffectDetails.MinSpeed          = 1;
    EffectDetails.MaxSlider2Val     = 100;
    EffectDetails.MinSlider2Val     = 1;
    EffectDetails.HasCustomSettings = true;
    EffectDetails.SupportsRandom    = false;

    SetSpeed(10);
}

Sequence::~Sequence()
{
    delete ui;
}

void Sequence::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void Sequence::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("Alternates colors with a fade effect").toStdString();
    EffectDetails.Slider2Name       = tr("Fade time").toStdString();
}

void Sequence::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    std::vector<RGBColor> colors = ui->colorsPicker->Colors();
    unsigned int colors_count = colors.size();
    unsigned int current_color_index = ((int)ceil(progress)) % colors_count;

    float whole;
    float frac = std::modf(progress, &whole);

    float fade_mult;

    RGBColor color;

    if(frac >= 0.8)
    {
       unsigned int next_color_index = current_color_index < colors_count - 1 ? current_color_index + 1 : 0;
       color = ColorUtils::Interpolate(colors[current_color_index], colors[next_color_index], (frac - 0.8) * 5);
       fade_mult = 1.f / (float) Slider2Val;
    }
    else
    {
        color = ui->colorsPicker->Colors()[current_color_index];
        fade_mult = 1.f;
    }

    for(ControllerZone* controller_zone : controller_zones)
    {
        controller_zone->SetAllZoneLEDs(color, Brightness, Temperature, Tint);
    }

    progress += fade_mult * 0.1 * (float) Speed / (float) FPS;
}


void Sequence::LoadCustomSettings(json settings)
{
    if (settings.contains("colors"))
    {
        ui->colorsPicker->SetColors(settings["colors"]);
    }
}

json Sequence::SaveCustomSettings()
{
    json settings;

    settings["colors"] = ui->colorsPicker->Colors();

    return settings;
}
