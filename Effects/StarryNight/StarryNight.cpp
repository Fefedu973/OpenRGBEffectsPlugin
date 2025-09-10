#include "StarryNight.h"

REGISTER_EFFECT(StarryNight);

StarryNight::StarryNight(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::StarryNight)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.EffectDescription = "Light random stars that fade in and out of the background";
    EffectDetails.HasCustomSettings = true;
    EffectDetails.SupportsRandom = true;
    SetSpeed(50);

    // Set the seed for the random number generators
    generator.seed(static_cast<unsigned int>( time(NULL) % 1000 ));
    srand(time(NULL));

    // Set default colors so they aren't both black.
    ui->backColor->SetRGBColor(ColorUtils::OFF());
    ui->colorsPickerStar->SetColors({ColorUtils::RandomRGBColor()});
}

void StarryNight::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        SetDynamicStrings();
    }
}

void StarryNight::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("Selects a random LED and fades it in an out").toStdString();
    EffectDetails.Slider2Name       = tr("Star Count").toStdString();
}

// This is implemented differently than most other effects.  Since only the active stars have colors other than background,
// we will loop through the stars instead of all LEDs.
void StarryNight::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    if (!controller_zones.empty())
    {
        if (backgroundColorChanged)
        {
            // The background color was changed. Loop through all controllers and set the color.
            // Stars will remain as is but change based on their current fade status to use the new background color
            for (std::size_t idx = 0; idx < controller_zones.size(); idx++)
            {
                //controller_zones[idx]->SetAllZoneLEDs(ui->backColor->CurrentRGBColor(), Brightness, Temperature, Tint);
                controller_zones[idx]->SetAllZoneLEDs(ColorUtils::apply_brightness(ui->backColor->CurrentRGBColor(), float(ui->backColorBrightness->value()) / 100), Brightness, Temperature, Tint);
            }
            backgroundColorChanged = false;
        }

        UpdateStarInfo(controller_zones);
        for(std::size_t i = 0; i < stars.size(); i++)
        {
            // update the colors for LEDs that are stars
            controller_zones[stars[i].controllerIdx]->SetLED(stars[i].ctrlLedIdx, stars[i].intColor, Brightness, Temperature, Tint);
        }
    }
}

// This function is used to resize the stars vector as needed.
void StarryNight::updateStarVectorSize(int starNum, int newDensity)
{
    //the star dentisty is changed or no stars exist so we need to create them
    if (stars.empty() || starNum > (int)stars.size())
    {
        // The number of stars required is either being set or has increased so we can safely adjust the vector size.
        stars.resize(starNum);
        if (density == newDensity)
        {
            // We are done our work so we can set densityChanged and starSize change.
            densityChanged = false;
        } // The implied else clause is that the density has changed before we finished this so we will need to go through these checks again at the next stepEffect.
    }
    else
    {
        // We are reducing the number of active stars so we have to stop those.
        for (int idx = (int)(stars.size() -1); idx > starNum -1; idx--)
        {
            inactiveStarToHandle = false;
            if (stars[idx].starStatus != inactive)
            {
                //Since we are removing these stars, set the status to inactive.
                stars[idx].starStatus = inactive;
                // We have at least one inactive star.  We need to make sure that it is handled before we resize the vector
                inactiveStarToHandle = true;
            }
        }

        if(!inactiveStarToHandle)
        {
            // We have gone through all the stars to remove and they are all inactive.
            // This means that the last stepEffect has set the color to backColor so we can resize the vector.
            if(newDensity == density)
            {
                stars.resize(starNum);
                inactiveStarToHandle = false;
                densityChanged = false;
            } // implied else clause is that the density value changed while we were handling the last change.
        }
    }
    if (!densityChanged)
    {
        ui->starDensity->setValue(density);
    }
}


// This function will manage the stars including: creating new stars, transitioning them through their states and setting background color (used in stepEffect).
void StarryNight::UpdateStarInfo(std::vector<ControllerZone*> /*controller_zones*/)
{
    // random number ranges.
    std::uniform_int_distribution<std::mt19937::result_type> newStarGenerator(0,totalLEDs); // set range based on max
    int newDensity = density;
    int starNum = ceil(double(totalLEDs) * double(newDensity) / 100);

    if (totalLEDs > 0)
    {
        // if (densityChanged || stars.empty())
        if (densityChanged || starNum != (int)stars.size())
        {
            // We need to update the star vector
            updateStarVectorSize(starNum, newDensity);
        }

        // Now we need to see what we need to do for each star entry.
        for (std::size_t idx = 0; idx < stars.size(); idx++)
        {   
            if (stars[idx].starStatus == off || stars.size() < idx)
            {
                // This star is either off or doesn't exist yet.
                bool starCheckInProgress = true;

                while (starCheckInProgress)
                {
                    // Generate a random star based on the number of LEDs
                    int newStarIdx = newStarGenerator(generator);
                    std::size_t idx1 = 0;
                    bool matchFound = false;

                    // Let's make sure that we haven't selected an LED that is already in the star vector
                    while((idx1 < stars.size()) && matchFound == false)
                    {
                        if (stars[idx1].ledIdx == newStarIdx)
                        {
                            //The LED is already in the star table so we need to generate a new one
                            matchFound = true;
                        }
                        idx1++;
                    }
                    if (matchFound == false)
                    {
                        //add the information for the newly created Star
                        if (RandomColorsEnabled)
                        {
                            stars[idx].color = ColorUtils::RandomRGBColor();
                        }
                        else
                        {
                            std::uniform_int_distribution<std::mt19937::result_type> starColorGenerator(0, (int)ui->colorsPickerStar->Colors().size() -1);
                            //stars[idx].color = starColors[starColorGenerator(generator)];
                            stars[idx].color = ui->colorsPickerStar->Colors().at(starColorGenerator(generator));
                        }
                        stars[idx].intColor = ColorUtils::apply_brightness(ui->backColor->CurrentRGBColor(), float(ui->backColorBrightness->value()) / 100);
                        stars[idx].starDelayStartTime = std::chrono::system_clock::now();
                        std::uniform_real_distribution<double> randomDelayGenerator(minDelayTime/10, maxDelayTime/10);
                        stars[idx].starDelay = std::chrono::milliseconds(int(randomDelayGenerator(generator) * 10000));
                        std::uniform_real_distribution<double> randomOnGenerator(minOnTime/10, maxOnTime/10);
                        stars[idx].starOnPeriod = std::chrono::milliseconds(int(randomOnGenerator(generator) * double(starOnTimeSpeed) / onRangeSelector * 10000));
                        stars[idx].starStatus = delayed;
                        stars[idx].ledIdx = newStarIdx;
                        stars[idx].controllerIdx = ledMapForStars[newStarIdx].controllerIdx;
                        stars[idx].ctrlLedIdx = ledMapForStars[newStarIdx].ledIndex;

                        // Since we we able to find a valid new star, stop the loop.
                        starCheckInProgress = false;
                    }
                }
            }
            // The star is in a delayed state to ensure that they don't all turn on at the same time.
            else if (stars[idx].starStatus == delayed)
            {
                if (stars[idx].starDelay < (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - stars[idx].starDelayStartTime)))
                {
                    //We can start to fade in this star to on since the delay period has been reached
                    std::uniform_real_distribution<double>randomFadeGenerator(minFadeTime/10.f, maxFadeTime/10.f);
                    stars[idx].starStatus = fadeIn;
                    stars[idx].starFadePeriod = std::chrono::milliseconds(int(randomFadeGenerator(generator) * double(fadeInSpeed) / fadeRangeSelector * 10000));
                    stars[idx].starFadeStartTime = std::chrono::system_clock::now();

                }
            }
            else if (stars[idx].starStatus == fadeIn)
            {
                if (stars[idx].starFadePeriod < std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - stars[idx].starFadeStartTime))
                {
                    //The star has finished fading in.  It is now considered as on
                    stars[idx].starStatus = on;
                    stars[idx].starOnStartTime = std::chrono::system_clock::now();
                    stars[idx].intColor = stars[idx].color;
                }
                else {
                    //Fade in the star
                    double fadeInMult = float((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - stars[idx].starFadeStartTime)).count()) / stars[idx].starFadePeriod.count();
                    stars[idx].intColor = ColorUtils::Interpolate(ColorUtils::apply_brightness(ui->backColor->CurrentRGBColor(), float(ui->backColorBrightness->value()) / 100), stars[idx].color, fadeInMult);
                }
            }
            else if (stars[idx].starStatus == on)
            {
                if (stars[idx].starOnPeriod < (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - stars[idx].starOnStartTime)))
                {
                    //This star is ready to start fading out
                    std::uniform_real_distribution<double> randomFadeGenerator(minFadeTime/10.f, maxFadeTime/10.f);
                    stars[idx].starStatus = fadeOut;
                    stars[idx].starFadePeriod = std::chrono::milliseconds(int(randomFadeGenerator(generator) * double(fadeOutSpeed) / fadeRangeSelector * 10000));
                    stars[idx].starFadeStartTime = std::chrono::system_clock::now();
                }
            }
            else if (stars[idx].starStatus == fadeOut)
            {
                if (stars[idx].starFadePeriod < std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - stars[idx].starFadeStartTime))
                {
                    //The star has finished fading out.  It is now considered as off
                    stars[idx].starStatus = off;
                    stars[idx].intColor = ColorUtils::apply_brightness(ui->backColor->CurrentRGBColor(), float(ui->backColorBrightness->value()) / 100);
                }
                else {
                    //Fade in the star
                    double fadeOutMult = float((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - stars[idx].starFadeStartTime)).count()) / stars[idx].starFadePeriod.count();
                    stars[idx].intColor = ColorUtils::Interpolate(stars[idx].color, ColorUtils::apply_brightness(ui->backColor->CurrentRGBColor(), float(ui->backColorBrightness->value()) / 100), fadeOutMult);
                }
            }
            else if (stars[idx].starStatus == inactive)
            {
                // The star is being removed so set the color to background color
                stars[idx].intColor = ColorUtils::apply_brightness(ui->backColor->CurrentRGBColor(), float(ui->backColorBrightness->value()) / 100);
            }
        }
    }
}

// This function adds all LEDs within the map
void StarryNight::UpdateStarIndexMap(std::vector<ControllerZone*> controller_zones)
{
    if (!controller_zones.empty())
    {
        // Since we had a change to the controller information, we need to clear the map
        ledMapForStars.clear();
        //Before we do anything, let's see how many controllers we have and how many LEDs they hold
        int mapIdx = 0;
        // We now have a mapping of all LEDs.  We need to activate those that
        for(unsigned int controllerIdx = 0; controllerIdx < controller_zones.size(); controllerIdx++)
        {
            for (unsigned int ledIdx =0; ledIdx < controller_zones[controllerIdx]->leds_count(); ledIdx++)
            {
                ledMapForStars[mapIdx] = ControllerMap(controllerIdx, ledIdx);
                mapIdx++;
            }
        }
        totalLEDs = ledMapForStars.size();
    }
}

void StarryNight::ResetStarryNight(std::vector<ControllerZone*> /*controller_zones*/)
{
    stars.clear();
    ledMapForStars.clear();
}

void StarryNight::OnControllerZonesListChanged(std::vector<ControllerZone*> controller_zones)
{
        for(std::size_t idx = 0; idx < controller_zones.size(); idx++)
        {
            // Set the color as the background color
            controller_zones[idx]->SetAllZoneLEDs(ColorUtils::apply_brightness(ui->backColor->CurrentRGBColor(), float(ui->backColorBrightness->value()) / 100), Brightness, Temperature, Tint);
        }
        // Since the controllers changed, we need to reset the mapping and star information as they may no longer be valid
        ResetStarryNight(controller_zones);
        UpdateStarIndexMap(controller_zones);
}

// Because we have default values, we will seet them here if nothing has been saved.
void StarryNight::LoadCustomSettings(json settings)
{
    if (settings.contains("starColors"))
    {
        ui->colorsPickerStar->SetColors(settings["starColors"]);
    }
    if (settings.contains("starDensity"))
    {
        ui->starDensity->setValue(settings["starDensity"]);
        density = ui->starDensity->value();
    }
    if(settings.contains("backgroundColors"))
    {
        ui->backColor->SetRGBColor(settings["backgroundColors"]);
    }
    if (settings.contains("fadeInSpeed"))
    {
        ui->fadeInSpeed->setValue(settings["fadeInSpeed"]);
        fadeInSpeed = ui->fadeInSpeed->value();
    }
    if (settings.contains("fadeOutSpeed"))
    {
        ui->fadeOutSpeed->setValue(settings["fadeOutSpeed"]);
        fadeOutSpeed = ui->fadeOutSpeed->value();
    }
    if (settings.contains("starOnTime"))
    {
        ui->starOnTime->setValue(settings["starOnTime"]);
        starOnTimeSpeed = ui->starOnTime->value();
    }
    if(settings.contains("backColorBrightness"))
    {
        ui->backColorBrightness->setValue(settings["backColorBrightness"]);
    }
}

json StarryNight::SaveCustomSettings()
{
    json settings;

    settings["starColors"] = ui->colorsPickerStar->Colors();
    settings["starDensity"] = density;
    settings["backgroundColors"] = ui->backColor->CurrentRGBColor();
    settings["fadeInSpeed"] = fadeInSpeed;
    settings["fadeOutSpeed"] = fadeOutSpeed;
    settings["starOnTime"] = starOnTimeSpeed;
    settings["backColorBrightness"] = ui->backColorBrightness->value();
    return settings;
}

void StarryNight::on_backColor_ColorSelected(QColor /*c*/)
{
    backgroundColorChanged = true;
}

void StarryNight::on_colorsPickerStar_ColorsChanged()
{
    //starColors = ui->colorsPickerStar->Colors();
}
void StarryNight::on_starDensity_valueChanged(int value)
{
    density = value;
    densityChanged = true;
}

void StarryNight::on_fadeInSpeed_valueChanged(int value)
{
    fadeInSpeed = value;
}
void StarryNight::on_fadeOutSpeed_valueChanged(int value)
{
    fadeOutSpeed = value;
}
void StarryNight::on_starOnTime_valueChanged(int value)
{
    starOnTimeSpeed = value;
}

void StarryNight::on_backColorBrightness_valueChanged(int /*value*/)
{
    //This is the same as changing the background color since we are changing it's brightness so we will reuse the same bool
    backgroundColorChanged = true;
}
