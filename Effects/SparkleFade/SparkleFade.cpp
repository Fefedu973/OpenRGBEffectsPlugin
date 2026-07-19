/*---------------------------------------------------------*\
| SparkleFade.cpp                                           |
|                                                           |
|   OpenRGB Effects Plugin Sparkle Fade Effect              |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "SparkleFade.h"
#include "ui_SparkleFade.h"

REGISTER_EFFECT(SparkleFade);

SparkleFade::SparkleFade(QWidget *parent)
    : RGBEffect(parent)
    , ui(new Ui::SparkleFade)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.UserColors        = 1;
    EffectDetails.HasCustomSettings = true;
    EffectDetails.SupportsRandom    = true;

    SetSpeed(100);

    // Set the seed for the random number generators
    generator.seed(static_cast<unsigned int>( time(NULL) % 1000 ));
    srand(time(NULL));
}

SparkleFade::~SparkleFade()
{
    delete ui;
}

void SparkleFade::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void SparkleFade::UpdateColors(std::vector<ControllerZone*> controller_zones)
{
    std::uniform_real_distribution<double> randomFadeStartGenerator(0, double(fadeOutTime.count()) /20000); //20000 because we want to at most to delay the fade period by half the user setting for the fade out period.
    std::uniform_real_distribution<double> radomFadeDelay(0, double(fadeOutTime.count()) /20000);

    if (currentEffectState == off)
    {
        if (offTime < std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - offStartTime))
        {
            // The time to stay off as expired.
            currentEffectState = fadeIn;
            fadeInStartTime = std::chrono::system_clock::now();

            // Set the base color for the next iteration of effects.
            if (RandomColorsEnabled)
            {
                baseColor = ColorUtils::RandomRGBColor();
            }
            else
            {
                baseColor = UserColors[0];
            }
        }
    }
    else if (currentEffectState == fadeIn)
    {
        if (fadeIntime < std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - fadeInStartTime))
        {
            currentEffectState = on;
        }
        else
        {
            double fadeInMult = float((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - fadeInStartTime)).count()) / fadeIntime.count();
            RGBColor tmpColor = ColorUtils::Interpolate(ColorUtils::OFF(), ColorUtils::apply_brightness(baseColor, float(Brightness) / 100), fadeInMult);
            for (unsigned int ctrlIdx = 0; ctrlIdx < controller_zones.size(); ctrlIdx++)
            {
                for (unsigned int ledIdx = 0; ledIdx < ledInfo[ctrlIdx].size(); ledIdx++)
                {
                    ledInfo[ctrlIdx][ledIdx].color = tmpColor;
                }
            }
        }
    }

    else if (currentEffectState ==  on)
    {
        std::chrono::time_point startTime = std::chrono::system_clock::now();
        //std::chrono::time_point  fadeEndTime = startTime + fadeOutTime;

        // Set the fade out information.
        for (unsigned int ctrlIdx = 0; ctrlIdx < ledInfo.size(); ctrlIdx++)
        {
            for (unsigned int ledIdx = 0; ledIdx < ledInfo[ctrlIdx].size(); ledIdx++)
            {
                int startDelay = int(randomFadeStartGenerator(generator) * 10000);
                ledInfo[ctrlIdx][ledIdx].fadeStartTime = startTime + std::chrono::milliseconds(startDelay);
                ledInfo[ctrlIdx][ledIdx].color = baseColor;

                // Set the fade time.
                //ledInfo[ctrlIdx][ledIdx].fadeTimePeriod = std::chrono::duration_cast<std::chrono::milliseconds>(fadeEndTime - ledInfo[ctrlIdx][ledIdx].fadeStartTime);
                ledInfo[ctrlIdx][ledIdx].fadeTimePeriod = std::chrono::milliseconds(int(radomFadeDelay(generator) * 10000));
            }
        }
        currentEffectState = fadeOut;
    }

    else if (currentEffectState == fadeOut)
    {
        bool fadeOutComplete = true;

        for (unsigned int ctrlIdx = 0; ctrlIdx < ledInfo.size(); ctrlIdx++)
        {
            for (unsigned int ledIdx = 0; ledIdx < ledInfo[ctrlIdx].size(); ledIdx++)
            {
                // The fade out information has already been set so we can handle the fade until
                if (ledInfo[ctrlIdx][ledIdx].fadeTimePeriod < std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - ledInfo[ctrlIdx][ledIdx].fadeStartTime))
                {
                    // The LED has fade out
                    ledInfo[ctrlIdx][ledIdx].color = ColorUtils::OFF();
                }
                else
                {
                    // Since we have at least one LED that isn't off.
                    fadeOutComplete = false;

                    if (std::chrono::system_clock::now() >= ledInfo[ctrlIdx][ledIdx].fadeStartTime)
                    {
                        //the LED is ready to start fading.
                        double fadeOutMult = float((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - ledInfo[ctrlIdx][ledIdx].fadeStartTime)).count()) / ledInfo[ctrlIdx][ledIdx].fadeTimePeriod.count();
                        ledInfo[ctrlIdx][ledIdx].color = ColorUtils::Interpolate(ColorUtils::apply_brightness(baseColor, float(Brightness) / 100), ColorUtils::OFF(), fadeOutMult);
                    }
                }
            }
        }

        if (fadeOutComplete)
        {
            // All LEDs have faded out
            currentEffectState = off;
            offStartTime = std::chrono::system_clock::now();
        }
    }
}

void SparkleFade::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    UpdateColors(controller_zones);

    for(unsigned int i = 0; i < controller_zones.size(); i++)
    {
        zone_type ZT = controller_zones[i]->type();

        if(ledInfo[i].size() != controller_zones[i]->size())
        {
            ledInfo[i].resize(controller_zones[i]->size());
        }

        if (ZT == ZONE_TYPE_SINGLE || ZT == ZONE_TYPE_LINEAR)
        {
            unsigned int leds_count = controller_zones[i]->leds_count();

            for (unsigned int LedID = 0; LedID < leds_count; LedID++)
            {
                controller_zones[i]->SetLED(LedID, ledInfo[i][LedID].color, Brightness, Temperature, Tint);
            }
        }

        else if (ZT == ZONE_TYPE_MATRIX)
        {
            int cols = controller_zones[i]->matrix_map_width();
            int rows = controller_zones[i]->matrix_map_height();

            for (int col_id = 0; col_id < cols; col_id++)
            {
                for (int row_id = 0; row_id < rows; row_id++)
                {
                    int idx = (row_id * cols) + col_id;
                    int LedID = controller_zones[i]->map()[idx];
                    controller_zones[i]->SetLED(LedID, ledInfo[i][LedID].color, Brightness, Temperature, Tint);
                }
            }

        }

    }
}


void SparkleFade::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("Fading in and fade out with sparkles").toStdString();
}


void SparkleFade::ResetSparkleFade(std::vector<ControllerZone*> controller_zones)
{
    ledInfo.clear();

    for(ControllerZone* controller_zone: controller_zones)
    {
        std::vector<ledDetails> zone_leds;
        zone_leds.resize(controller_zone->size());
        ledInfo.push_back(zone_leds);
    }
}

void SparkleFade::OnControllerZonesListChanged(std::vector<ControllerZone*> controller_zones)
{
    ResetSparkleFade(controller_zones);
    if (!controller_zones.empty())
    {
        for (unsigned int ctrlIdx = 0; ctrlIdx < controller_zones.size(); ctrlIdx++)
        {
            controller_zones[ctrlIdx]->SetAllZoneLEDs(ColorUtils::OFF(), Brightness, Temperature, Tint);
        }
    }
}

void SparkleFade::on_offTime_valueChanged(int value)
{
    offTime = std::chrono::milliseconds(value);
}

void SparkleFade::on_fadeInTime_valueChanged(int value)
{
    fadeIntime = std::chrono::milliseconds(value);
}

void SparkleFade::on_fadeOutTime_valueChanged(int value)
{
    fadeOutTime = std::chrono::milliseconds(value);
}

void SparkleFade::LoadCustomSettings(json settings)
{
    if (settings.contains("StopTime"))
    {
        ui->offTime->setValue(settings["StopTime"]);
    }

    if (settings.contains("FadeInTime"))
    {
        ui->fadeInTime->setValue(settings["FadeInTime"]);
    }

    if (settings.contains("FadeOutTime"))
    {
        ui->fadeOutTime->setValue(settings["FadeOutTime"]);
    }
}

json SparkleFade::SaveCustomSettings()
{
    json settings;
    settings["StopTime"] = ui->offTime->value();
    settings["FadeInTime"] = ui->fadeInTime->value();
    settings["FadeOutTime"] = ui->fadeOutTime->value();

    return settings;
}
