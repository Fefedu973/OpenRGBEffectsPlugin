/*---------------------------------------------------------*\
| MotionPoint.cpp                                           |
|                                                           |
|   OpenRGB Effects Plugin Motion Point Effect              |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "MotionPoint.h"

REGISTER_EFFECT(MotionPoint);

MotionPoint::MotionPoint(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::MotionPoint)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.MaxSpeed          = 50;
    EffectDetails.MinSpeed          = 1;
    EffectDetails.UserColors        = 1;
    EffectDetails.HasCustomSettings = true;

    ui->background->SetRGBColor(background);

    SetSpeed(25);
}

MotionPoint::~MotionPoint()
{
    delete ui;
}

void MotionPoint::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void MotionPoint::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("A point that moves forth and back on your devices").toStdString();
}

void MotionPoint::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    double t = (1 + sin(progress)) / 2.f;

    for(ControllerZone* controller_zone: controller_zones)
    {
        zone_type ZT = controller_zone->type();
        int leds_count = controller_zone->leds_count();

        if((ZT == ZONE_TYPE_SINGLE)
        || (ZT == ZONE_TYPE_LINEAR)
        || (ZT == ZONE_TYPE_LINEAR_LOOP))
        {
            for(int LedID = 0; LedID < leds_count; LedID++)
            {
                RGBColor color = GetColor(leds_count, LedID, t);
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
                RGBColor color = GetColor(cols, col_id, t);

                for(int row_id = 0; row_id < rows; row_id++)
                {
                    int LedID = controller_zone->map()[((row_id * cols) + col_id)];
                    controller_zone->SetLED(LedID, color, Brightness, Temperature, Tint);
                }
            }
        }
    }

    progress += 0.1 * (float) Speed / (float) FPS;
}

RGBColor MotionPoint::GetColor(unsigned int w, unsigned int x, double t)
{
    if(w == 0)
    {
        w = 1;
    }

    float distance = fabs(x - t * (w-1));

    if(distance > 2)
    {
        return background;
    }

    if(RandomColorsEnabled)
    {
        if(t <= 0.0005 || t >= 0.9995)
        {
            current  = ColorUtils::RandomRGBColor();
        }
    }
    else
    {
        current = UserColors[0];
    }

    return ColorUtils::Interpolate(current, background, distance / 2.f);
}

void MotionPoint::LoadCustomSettings(json settings)
{
    if(settings.contains("background"))
    {
        background = settings["background"];
        ui->background->SetRGBColor(background);
    }
}

json MotionPoint::SaveCustomSettings()
{
    json settings;

    settings["background"] = background;

    return settings;
}

void MotionPoint::on_background_ColorSelected(QColor color)
{
    background = ColorUtils::fromQColor(color);
}
