/*---------------------------------------------------------*\
| Clock.cpp                                                 |
|                                                           |
|   OpenRGB Effects Plugin Clock Effect                     |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "Clock.h"

REGISTER_EFFECT(Clock);

Clock::Clock(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::Clock)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName = ClassName();
    EffectDetails.UserColors = 3;
    EffectDetails.SupportsRandom = false;
    EffectDetails.HasCustomSettings = true;
}

Clock::~Clock()
{
    delete ui;
}

void Clock::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void Clock::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("Digital Clock").toStdString();
    ui->clock_mode_combo_box->clear();
    ui->clock_mode_combo_box->addItems({tr("12-hour"),
                                        tr("24-hour")});
}

#ifdef WIN32
#define localtime_r(_Time, _Tm) localtime_s(_Tm, _Time)
#endif

void Clock::StepEffect(std::vector<ControllerZone*> controller_zones)
{    
    tm localTime;
    std::chrono::system_clock::time_point t = std::chrono::system_clock::now();
    time_t now = std::chrono::system_clock::to_time_t(t);
    localtime_r(&now, &localTime);

    const std::chrono::duration<double> tse = t.time_since_epoch();
    std::chrono::seconds::rep milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(tse).count() % 1000;

    int mode = clock_mode == CLOCK_MODE_12_HOUR ? 12 : 24;

    s = localTime.tm_sec + milliseconds * 0.001;
    m = localTime.tm_min + localTime.tm_sec / 60.;
    h = (localTime.tm_hour % mode) + localTime.tm_min / 60.;

    for(ControllerZone* controller_zone : controller_zones)
    {
        zone_type ZT = controller_zone->type();
        bool reverse = controller_zone->reverse;

        if(ZT == ZONE_TYPE_SINGLE || ZT == ZONE_TYPE_LINEAR)
        {
            unsigned int width = controller_zone->leds_count();

            for(unsigned int i = 0; i < width; i++)
            {
                RGBColor color = GetColor(reverse ? width - i - 1 : i, width, mode);
                controller_zone->SetLED(i, color, Brightness, Temperature, Tint);
            }

        }
        else if(ZT == ZONE_TYPE_MATRIX)
        {
            int cols = controller_zone->matrix_map_width();
            int rows = controller_zone->matrix_map_height();

            for (int col_id = 0; col_id < cols; col_id++)
            {
                RGBColor color = GetColor(reverse ? cols - col_id - 1: col_id, cols, mode);

                for (int row_id = 0; row_id < rows; row_id++)
                {
                    int LedID = controller_zone->map()[((row_id * cols) + col_id)];
                    controller_zone->SetLED(LedID, color, Brightness, Temperature, Tint);
                }
            }
        }
    }
}

RGBColor Clock::GetColor(float x, float w, float mode)
{
    float step_h = (w-1) * h / mode; // [0-w]
    float step_m = (w-1) * m / 60.0; // [0-w]
    float step_s = (w-1) * s / 60.0; // [0-w]

    RGBColor hours_coulor = 0;
    RGBColor minutes_coulor = 0;
    RGBColor seconds_coulor = 0;

    if(fabs(x - step_h) <= 1)
    {
        if(x < step_h)
        {
            hours_coulor = ColorUtils::apply_brightness(UserColors[0], 1-(step_h - x));
        }
        else
        {
            hours_coulor = ColorUtils::apply_brightness(UserColors[0], 1-(x - step_h));
        }
    }

    if(fabs(x - step_m) <= 1)
    {
        if(x < step_m)
        {
            minutes_coulor = ColorUtils::apply_brightness(UserColors[1], 1-(step_m - x));
        }
        else
        {
            minutes_coulor = ColorUtils::apply_brightness(UserColors[1], 1-(x - step_m));
        }
    }

    if(fabs(x - step_s) <= 1)
    {
        if(x < step_s)
        {
            seconds_coulor = ColorUtils::apply_brightness(UserColors[2], 1-(step_s - x));
        }
        else
        {
            seconds_coulor = ColorUtils::apply_brightness(UserColors[2], 1-(x - step_s));
        }
    }

    return ColorUtils::Lighten(ColorUtils::Lighten(hours_coulor, minutes_coulor), seconds_coulor);
}

void Clock::on_clock_mode_combo_box_currentIndexChanged(int value)
{
    clock_mode = value;
}


void Clock::LoadCustomSettings(json settings)
{
    if(settings.contains("clock_mode"))  clock_mode  = settings["clock_mode"];
    ui->clock_mode_combo_box->setCurrentIndex(clock_mode);
}

json Clock::SaveCustomSettings()
{
    json settings;
    settings["clock_mode"]       = clock_mode;
    return settings;
}
