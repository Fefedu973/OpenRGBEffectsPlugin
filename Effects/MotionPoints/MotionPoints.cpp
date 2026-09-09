/*---------------------------------------------------------*\
| MotionPoints.cpp                                          |
|                                                           |
|   OpenRGB Effects Plugin Motion Points Effect             |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "MotionPoints.h"
#include "ColorUtils.h"

REGISTER_EFFECT(MotionPoints);

MotionPoints::MotionPoints(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::MotionPoints)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.IsReversable      = true;
    EffectDetails.MaxSpeed          = 200;
    EffectDetails.MinSpeed          = 1;
    EffectDetails.UserColors        = 2;
    EffectDetails.MaxSlider2Val     = 100;
    EffectDetails.MinSlider2Val     = 1;

    SetSpeed(100);
    RGBEffect::SetSlider2Val(20);
}

MotionPoints::~MotionPoints()
{
    delete ui;
}

void MotionPoints::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void MotionPoints::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("Multiple points that moves in all directions on your devices").toStdString();
    EffectDetails.Slider2Name       = tr("Number of points").toStdString();
}

void MotionPoints::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    UpdatePoints();

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
                RGBColor color = GetColor(LedID, 0, leds_count, 1);
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
                    RGBColor color = GetColor(col_id, row_id, cols, rows);
                    int LedID = controller_zone->map()[((row_id * cols) + col_id)];
                    controller_zone->SetLED(LedID, color, Brightness, Temperature, Tint);
                }
            }
        }
    }
}

void MotionPoints::UpdatePoints()
{
    for(MovingPoint& point: points)
    {
        point.Move(0.0001 * Speed * point.speed_mult);

        if(point.IsOut())
        {
            point.Init(RandomColorsEnabled ? ColorUtils::RandomRGBColor() : UserColors[rand() % UserColors.size()]);
        }
    }
}

RGBColor MotionPoints::GetColor(float x, float y, float w, float h)
{
    for(MovingPoint& point: points)
    {
       float distance = sqrt(pow(point.x * w - x, 2) + pow(point.y * h - y, 2));

       if(distance <= 1)
       {
           return ColorUtils::Enlight(point.color, 1 - distance);
       }
    }

    return ColorUtils::OFF();
}

void MotionPoints::SetSlider2Val(unsigned int value)
{
    Slider2Val = value;
    points.clear();

    UserColors.resize(2);

    for(unsigned int i = 0; i < value; i++)
    {
        MovingPoint point;
        point.Init(RandomColorsEnabled ? ColorUtils::RandomRGBColor() : UserColors[rand() % UserColors.size()]);
        points.push_back(point);
    }
}
