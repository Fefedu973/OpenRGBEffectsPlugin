/*---------------------------------------------------------*\
| RandomMarquee.cpp                                         |
|                                                           |
|   OpenRGB Effects Plugin Random Marquee Effect            |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "RandomMarquee.h"
#include "ColorUtils.h"

REGISTER_EFFECT(RandomMarquee);

RandomMarquee::RandomMarquee(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::RandomMarquee)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.IsReversable      = true;
    EffectDetails.MaxSpeed          = 200;
    EffectDetails.MinSpeed          = 1;
    EffectDetails.UserColors        = 1;

    random.saturation = 255;
    random.value = 255;
    random.hue = 0;

    SetSpeed(50);
}

RandomMarquee::~RandomMarquee()
{
    delete ui;
}

void RandomMarquee::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void RandomMarquee::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("A simple Random Marquee for your devices").toStdString();
}

double RandomMarquee::custom_rand(double min, double max)
{
    double d = (double)rand() / RAND_MAX;
    return min + d * (max - min);
}

void RandomMarquee::StepEffect(std::vector<ControllerZone*> controller_zones)
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
                RGBColor color = GetColor(reverse ? leds_count - LedID - 1 : LedID);
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
                RGBColor color = GetColor(reverse ? cols - col_id - 1: col_id);

                for(int row_id = 0; row_id < rows; row_id++)
                {
                    int LedID = controller_zone->map()[((row_id * cols) + col_id)];
                    controller_zone->SetLED(LedID, color, Brightness, Temperature, Tint);
                }
            }

        }
    }

    progress += 0.005 * Speed * progress_mult / (float) FPS;

    unsigned int current_progress = (unsigned int) progress;

    // new cycle
    if(last_progress != current_progress)
    {
        last_progress = current_progress;
        speed_mult = custom_rand(0.5, 1.5);
        progress_mult = custom_rand(0.5, 1.5);

        dir = rand() % 2 == 0;
        spacing = 1 + rand() % 3;

        if(RandomColorsEnabled)
        {
            random.hue = rand() % 360;
        }
    }

}

RGBColor RandomMarquee::GetColor(unsigned int i)
{
    return (i + (int) ((dir?-1:1)*20.0*progress*speed_mult)) % (2*spacing) == 0 ?
                RandomColorsEnabled ?
                    RGBColor(hsv2rgb(&random)) : UserColors[0]
              : ColorUtils::OFF();
}
