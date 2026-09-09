/*---------------------------------------------------------*\
| ColorWheel.cpp                                            |
|                                                           |
|   OpenRGB Effects Plugin Color Wheel Effect               |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "ColorWheel.h"
#include "hsv.h"

REGISTER_EFFECT(ColorWheel);

ColorWheel::ColorWheel(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::ColorWheel)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.IsReversable      = true;
    EffectDetails.MaxSpeed          = 200;
    EffectDetails.MinSpeed          = 10;
    EffectDetails.HasCustomSettings = true;

    SetSpeed(100);
}

ColorWheel::~ColorWheel()
{
    delete ui;
}

void ColorWheel::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void ColorWheel::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("A rotating rainbow").toStdString();
    ui->direction->clear();
    ui->direction->addItems({tr("Clockwise"),
                             tr("Counter-clockwise")});
}

void ColorWheel::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    float cx_shift_mult = cx_shift / 100.f;
    float cy_shift_mult = cy_shift / 100.f;

    for(ControllerZone* controller_zone: controller_zones)
    {
        zone_type ZT = controller_zone->type();
        int leds_count = controller_zone->leds_count();
        bool reverse = controller_zone->reverse;

        if((ZT == ZONE_TYPE_SINGLE)
        || (ZT == ZONE_TYPE_LINEAR)
        || (ZT == ZONE_TYPE_LINEAR_LOOP))
        {
            double cx = leds_count * cx_shift_mult;
            double cy = 0;

            for(int LedID = 0; LedID < leds_count; LedID++)
            {
                RGBColor color = GetColor(LedID, 0, cx, cy, reverse);
                controller_zone->SetLED(LedID, color, Brightness, Temperature, Tint);
            }
        }

        else if((ZT == ZONE_TYPE_MATRIX)
             || (ZT == ZONE_TYPE_MATRIX_LOOP_X)
             || (ZT == ZONE_TYPE_MATRIX_LOOP_Y))
        {
            int cols = controller_zone->matrix_map_width();
            int rows = controller_zone->matrix_map_height();

            double cx = (cols - 1) * cx_shift_mult;
            double cy = (rows - 1) * cy_shift_mult;

            for(int col_id = 0; col_id < cols; col_id++)
            {
                for(int row_id = 0; row_id < rows; row_id++)
                {
                    RGBColor color = GetColor(col_id, row_id, cx, cy, reverse);
                    int LedID = controller_zone->map()[((row_id * cols) + col_id)];
                    controller_zone->SetLED(LedID, color, Brightness, Temperature, Tint);
                }
            }
        }
    }

    progress += (float) Speed / (float) FPS;
}


RGBColor ColorWheel::GetColor(unsigned int x, unsigned int y, double cx, double cy, bool reverse)
{
    float direction_mult = direction == 0 ? 1.f : -1.f;
    float hue = (float)(progress + (int)(180 + direction_mult * (reverse ? atan2(y - cy, x - cx) : atan2(x - cx, y - cy)) * (180.0 / 3.14159)) % 360);
    hsv_t hsv = { 0, 0, 0 };
    hsv.hue = (int)hue;
    hsv.saturation = 255;
    hsv.value = 255;

    return RGBColor(hsv2rgb(&hsv));
}

void ColorWheel::on_cx_valueChanged(int value)
{
    cx_shift = value;
}

void ColorWheel::on_cy_valueChanged(int value)
{
    cy_shift = value;
}

void ColorWheel::on_direction_currentIndexChanged(int value)
{
    direction = value;
}

void ColorWheel::LoadCustomSettings(json settings)
{
    if(settings.contains("cx")) ui->cx->setValue(settings["cx"]);
    if(settings.contains("cy")) ui->cy->setValue(settings["cy"]);
    if(settings.contains("direction")) ui->direction->setCurrentIndex(settings["direction"]);
}

json ColorWheel::SaveCustomSettings()
{
    json settings;

    settings["cx"] = cx_shift;
    settings["cy"] = cy_shift;
    settings["direction"] = direction;

    return settings;
}
