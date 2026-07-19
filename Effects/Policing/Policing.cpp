/*---------------------------------------------------------*\
| Policing.cpp                                              |
|                                                           |
|   OpenRGB Effects Plugin Policing Effect                  |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "Policing.h"
#include "ColorUtils.h"

REGISTER_EFFECT(Policing);

Policing::Policing() : RGBEffect()
{
    SetDynamicStrings();
    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.MaxSpeed          = 100;
    EffectDetails.MinSpeed          = 1;
    EffectDetails.UserColors        = 1;
    EffectDetails.MinSlider2Val     = 1;
    EffectDetails.MaxSlider2Val     = 100;

    SetSpeed(50);
    SetSlider2Val(20);

    C0 = ColorUtils::RandomRGBColor();
}

void Policing::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        SetDynamicStrings();
    }
}

void Policing::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("A back and forth effect motion with a flash before changing direction").toStdString();
    EffectDetails.Slider2Name       = tr("Width").toStdString();
}


void Policing::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    // Calculate a few things outside the loop
    Progress += 0.01 * float(Speed) / float(FPS);

    if (flashLength < 0)
    {
        flashLength = 1.f;
        last_step = step;
    }

    width = 0.01 * Slider2Val;          // [0-1] float, size of the visor
    h_width = 0.5 * width;              // [0-0.5] float, half width
    p = (Progress - (long) Progress);   // [0-1] float, 0 to 1 progress
    step = p < 0.5;                     // p in [0-0.5] = fist step, p in [0.5-1] = second step
    p_step =  step ? 2.*p : 2.*(1.- p); // [0-1] float, 0 to 1 progress within the step

    if (RandomColorsEnabled)
    {
        C0 = ColorUtils::RandomRGBColor();
    }
    else
    {
        C0 = UserColors[0];
    }

    bool flipping = (last_step != step);

    for(ControllerZone* controller_zone: controller_zones)
    {
        /*-------------------*\
        | Setup for the loop  |
        \*-------------------*/
        zone_type ZT = controller_zone->type();

        /*----------------------------------------------------*\
        | Adjust how it applies for the specific type of zone  |
        \*----------------------------------------------------*/
        if (ZT == ZONE_TYPE_SINGLE || ZT == ZONE_TYPE_LINEAR)
        {
            int leds_count = controller_zone->leds_count();

            for (int LedID = 0; LedID < leds_count; LedID++)
            {
                if (!flipping)
                {
                    controller_zone->SetLED(LedID, GetColor(LedID, leds_count), Brightness, Temperature, Tint);
                }
                else
                {
                    controller_zone->SetLED(LedID, C0, Brightness, Temperature, Tint);
                    flashLength = flashLength - 0.03 * Speed / FPS;
                }
            }
        }

        else if (ZT == ZONE_TYPE_MATRIX)
        {
            int cols = controller_zone->matrix_map_width();
            int rows = controller_zone->matrix_map_height();

            for (int col_id = 0; col_id < cols; col_id++)
            {
                RGBColor color;

                if (!flipping)
                {
                    color = GetColor(col_id, cols);
                }
                else
                {
                    color = C0;
                    flashLength = flashLength - 0.03 * Speed / FPS;
                }

                for (int row_id = 0; row_id < rows; row_id++)
                {
                    int LedID = controller_zone->map()[((row_id * cols) + col_id)];
                    controller_zone->SetLED(LedID, color, Brightness, Temperature, Tint);
                }
            }
        }
    }

}

RGBColor Policing::GetColor(float i, float count)
{
    // Constraint absolute minimum size
    float w = std::max<float>(1.5/count, width);
    float x_step = p_step * (1.+ 4.*w) - 1.5*w;

    // dont divide by zero
    if(count <= 1)
    {
        count = 2;
    }

    float x = i / (count-1);
    float dist = x_step - x;


    // fade the head
    if(dist < 0)
    {
        float l = std::max<float>(0.,std::min<float>((w+dist)/w, 1.));
        return step ? ColorUtils::Enlight(ColorUtils::OFF(), l) : ColorUtils::Enlight(C0, l) ;
    }

    // fade the tail
    if(dist > w)
    {
        float l = std::max<float>(0.,std::min<float>(1.-((dist-w)/w), 1.));
        return step ? ColorUtils::Enlight(C0, l) : ColorUtils::Enlight(ColorUtils::OFF(), l) ;
    }

    // interpolate colors

    float interp = std::min<float>(std::max<float>((w-dist)/w, 0.),1.);

    return step ?
               ColorUtils::Interpolate(C0,ColorUtils::OFF(),interp):
               ColorUtils::Interpolate(ColorUtils::OFF(),C0,interp);

}
