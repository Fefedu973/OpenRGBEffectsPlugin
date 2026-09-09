/*---------------------------------------------------------*\
| DoubleRotatingRainbow.cpp                                 |
|                                                           |
|   OpenRGB Effects Plugin Double Rotating Rainbow Effect   |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "DoubleRotatingRainbow.h"

REGISTER_EFFECT(DoubleRotatingRainbow);

DoubleRotatingRainbow::DoubleRotatingRainbow(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::DoubleRotatingRainbow)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.IsReversable      = true;
    EffectDetails.MaxSpeed          = 100;
    EffectDetails.MinSpeed          = 1;
    EffectDetails.MaxSlider2Val     = 100;
    EffectDetails.MinSlider2Val     = 1;

    EffectDetails.HasCustomSettings = true;
    EffectDetails.SupportsRandom = false;

    SetSpeed(50);
    SetSlider2Val(20);
}

DoubleRotatingRainbow::~DoubleRotatingRainbow()
{
    delete ui;
}

void DoubleRotatingRainbow::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void DoubleRotatingRainbow::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("Two rainbows that rotate synchronously").toStdString();
    EffectDetails.Slider2Name       = tr("Color speed").toStdString();
}

void DoubleRotatingRainbow::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    for(ControllerZone* controller_zone: controller_zones)
    {
        zone_type ZT = controller_zone->type();
        int leds_count = controller_zone->leds_count();
        bool reverse = controller_zone->reverse;

        if((ZT == ZONE_TYPE_SINGLE)
        || (ZT == ZONE_TYPE_LINEAR)
        || (ZT == ZONE_TYPE_LINEAR_LOOP))
        {
            for(int LedID = 0; LedID < leds_count; LedID++)
            {
                RGBColor color = GetColor(LedID, 0.5, leds_count  * 0.5, 0.5, reverse);
                controller_zone->SetLED(LedID, color, Brightness, Temperature, Tint);
            }
        }

        else if((ZT == ZONE_TYPE_MATRIX)
             || (ZT == ZONE_TYPE_MATRIX_LOOP_X)
             || (ZT == ZONE_TYPE_MATRIX_LOOP_Y))
        {
            int cols = controller_zone->matrix_map_width();
            int rows = controller_zone->matrix_map_height();

            for(int col_id = 0; col_id < cols; col_id++)
            {
                for(int row_id = 0; row_id < rows; row_id++)
                {
                   RGBColor color = GetColor(col_id, row_id, (cols - 1) * 0.5, (rows - 1) * 0.5, reverse);
                   int LedID = controller_zone->map()[((row_id * cols) + col_id)];
                   controller_zone->SetLED(LedID, color, Brightness, Temperature, Tint);
                }
            }
        }
    }

    time += 0.01 * Speed / (float) FPS;

    cos_time = cos(time);
    sin_time = sin(time);

    cos_reverse_time = cos(-time);
    sin_reverse_time = sin(-time);
}

RGBColor DoubleRotatingRainbow::GetColor(float x, float y, float cx, float cy, bool reverse)
{
    double c = reverse? cos_reverse_time : cos_time;
    double s = reverse? sin_reverse_time : sin_time;

    hsv_t hsv;

    hsv.hue = (int)(time * Slider2Val + 360 * (frequency * (y - cy) * c + (56 - fabs(x - cx)) * frequency * s) / 128.0) % 360;
    hsv.value = 255;
    hsv.saturation = 255;

    return RGBColor(hsv2rgb(&hsv));
}

void DoubleRotatingRainbow::on_frequency_valueChanged(int value)
{
    frequency = value;
}

void DoubleRotatingRainbow::LoadCustomSettings(json settings)
{
    if(settings.contains("frequency")) ui->frequency->setValue(settings["frequency"]);
}

json DoubleRotatingRainbow::SaveCustomSettings()
{
    json settings;

    settings["frequency"] = frequency;

    return settings;
}
