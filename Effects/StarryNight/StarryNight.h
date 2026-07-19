/*---------------------------------------------------------*\
| StarryNight.h                                             |
|                                                           |
|   OpenRGB Effects Plugin Starry Night Effect              |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include <QEvent>
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include <QWidget>
#include "ui_StarryNight.h"
#include <random>
#include <chrono>
#include "ColorUtils.h"

namespace Ui {
class StarryNight;
}

enum StarState
{
    inactive, // The stat is being removed due to a reduction in density
    off, // Star is currently off
    delayed, // To ensure that all stars do not turn on at the same time, we introduce a delay
    fadeIn, // star is fading in
    on, // Star is at peak color
    fadeOut // Star is fading out
};

struct Star
{
    Star() :    color(ColorUtils::OFF()),
                intColor(ColorUtils::OFF()),
                starDelayStartTime(std::chrono::system_clock::now()),
                starDelay(std::chrono::milliseconds(0)),
                starOnStartTime(std::chrono::system_clock::now()),
                starOnPeriod(std::chrono::milliseconds(0)),
                starFadeStartTime(std::chrono::system_clock::now()),
                starFadePeriod(std::chrono::milliseconds(0)),
                //fadeOutMult(1.f),
                //fadeOutMultFactor(0),
                starStatus(off),
                controllerIdx(-1),
                ctrlLedIdx(-1),
                ledIdx(-1) {};

    RGBColor color = ColorUtils::OFF(); // color when star is on.  Used during fafe in and out
    RGBColor intColor = ColorUtils::OFF(); //Intermediate color used to set LED color for stars during stepEffect
    std::chrono::time_point<std::chrono::system_clock> starDelayStartTime; //start time of star delay
    std::chrono::milliseconds starDelay; // Randomly select time before the start fades in within min/max DelayTime
    std::chrono::time_point<std::chrono::system_clock> starOnStartTime; //Start time when star is considered on
    std::chrono::milliseconds starOnPeriod; //Ramdonly selected time that star will stay on within min/max OnTime
    std::chrono::time_point<std::chrono::system_clock> starFadeStartTime; //Start time when a star starts to fade in or out
    std::chrono::milliseconds starFadePeriod; //Ramdonly selected time that star will fade in or out on within min/max FadeTime
    StarState starStatus = off; // Current star status
    int controllerIdx; // Linked controller index
    int ctrlLedIdx; // Linked led on the controller
    int ledIdx = -1; //mapping index to ledMapForStars. Default value must be an invalid LED index otherwise that LED will never be activated.
};

// This structure is required to map star indexes to their proper controller/LED combination
// It is used as part of ledMapForStars map. Required since stars are selected randomly from all active stars.  The map and this struc allow us to retrieve the controller/led information.
struct ControllerMap
{
    ControllerMap() : controllerIdx(0), ledIndex(0) {};
    ControllerMap(int tControllerIdx, int tLedIndex) : controllerIdx(tControllerIdx), ledIndex(tLedIndex) {}
    int controllerIdx;
    int ledIndex;
};

class StarryNight: public RGBEffect
{
    Q_OBJECT

public:
    explicit StarryNight(QWidget *parent = nullptr);
    ~StarryNight() {};

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_ADVANCED, [](){return new StarryNight;});

    static std::string const ClassName() {return "StarryNight";}
    static std::string const UI_Name() { return QT_TR_NOOP("Starry Night"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void OnControllerZonesListChanged(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_backColor_ColorSelected(QColor);
    void on_colorsPickerStar_ColorsChanged();
    void on_starDensity_valueChanged(int);
    void on_fadeInSpeed_valueChanged(int);
    void on_fadeOutSpeed_valueChanged(int);
    void on_starOnTime_valueChanged(int);
    void on_backColorBrightness_valueChanged(int);

private:
    void SetDynamicStrings();

    // Used to ensure that LEDs do not all turn on at the same time.
    const double minDelayTime = 0.0; // seconds
    const double maxDelayTime = 1.0; // seconds

    // Possible enhancement: Could look at making this range user adjustable. Could use two sliders.  First sets min, second is max = min + max(value)
    // This is the range of seconds that each Star will be ON when starOnTime is 50%
    const double minOnTime = 1.0; // seconds
    const double maxOnTime = 3.0; // seconds
    const double onRangeSelector = 50.0; // At what value we want the range above to be used.

    // This is the range of time that each Star will take to fade in at 50% fadeInSpeed or fadeOutSpeed.
    // Possible enhancement: Could look at making this range user adjustable.
    const double minFadeTime = 1.0; //seconds
    const double maxFadeTime = 3.0; //seconds
    const double fadeRangeSelector = 50.0; // At what value we want the range above to be used.

    Ui::StarryNight *ui;
    std::vector<Star> stars;
    //std::vector<RGBColor> starColors = {ColorUtils::RandomRGBColor(), ColorUtils::RandomRGBColor()}; // Colors that stars will transition to
    bool inactiveStarToHandle = false; // Used to track if we are in the process of removing active stars (due to a density reduction)
    int density = 50; // Used to control the density of stars as a % of the total LEDs involved.  Note: if this is changed, ui file will also need ot change
    //RGBColor backColor = ColorUtils::OFF(); // The background color
    bool densityChanged = false; // This is required because density impacts the stars vector and we don't have access to the controller_zone when it is changed.
    bool backgroundColorChanged = false; // Similar to above but when the background color is changed.
    bool jsonValuesPresent = false; // Used to determine if saved values exist.
    unsigned int totalLEDs = 0; // Total # of LEDs selected across all controllers used for the effect
    int fadeInSpeed = 50; // The fade in speed selector Note: if this is changed, ui file will also need ot change
    int fadeOutSpeed = 50; // The fade out speed selector Note: if this is changed, ui file will also need ot change
    int starOnTimeSpeed = 50; // The speed selector for the  amount of time a star will stay at its brightest Note: if this is changed, ui file will also need ot change
    std::map<int, ControllerMap> ledMapForStars; // Map of selected star to controller/led combination
    std::mt19937 generator; // mersenne_twister_engine seeded with rd()

    void ResetStarryNight(std::vector<ControllerZone*>);
    void UpdateStarInfo(std::vector<ControllerZone*>);
    void UpdateStarIndexMap(std::vector<ControllerZone*>);
    void updateStarVectorSize(int, int);
};

/* Things to look at tracker
 *  POSSIBLE CHANGE: Should we look at moving UpdateStarInfo into StepEffect? It is called upon a controller list change but we could handle that the same way as a change to the background color.
 */
