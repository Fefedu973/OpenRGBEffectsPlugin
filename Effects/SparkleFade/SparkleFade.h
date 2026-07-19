/*---------------------------------------------------------*\
| SparkleFade.h                                             |
|                                                           |
|   OpenRGB Effects Plugin Sparkle Fade Effect              |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "ui_SparkleFade.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include <QWidget>
#include <random>
#include <chrono>

namespace Ui {
class SparkleFade;
}

class SparkleFade : public RGBEffect
{
    Q_OBJECT

    enum effectState
    {
        fadeIn,
        on,
        fadeOut,
        off
    };

    struct ledDetails
    {
        ledDetails() :  fadeStartTime(std::chrono::system_clock::now()),
            fadeTimePeriod(std::chrono::milliseconds(500)),
            color(ColorUtils::OFF()) {};

        std::chrono::time_point<std::chrono::system_clock> fadeStartTime;
        std::chrono::milliseconds fadeTimePeriod;
        RGBColor color;
    };

public:
    explicit SparkleFade(QWidget *parent = nullptr);
    ~SparkleFade();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SIMPLE, [](){return new SparkleFade;});

    static std::string const ClassName() { return "SparkleFade"; }
    static std::string const UI_Name() { return QT_TR_NOOP("Sparkle Fade"); }

    void StepEffect(std::vector<ControllerZone*> controller_zones);
    void OnControllerZonesListChanged(std::vector<ControllerZone*> controller_zones);
    void LoadCustomSettings(json settings);
    json SaveCustomSettings();

private slots:
    void changeEvent(QEvent *event);
    void on_offTime_valueChanged(int value);
    void on_fadeInTime_valueChanged(int value);
    void on_fadeOutTime_valueChanged(int value);

private:
    std::chrono::milliseconds fadeOutTime = std::chrono::milliseconds(10000);
    std::chrono::milliseconds fadeIntime = std::chrono::milliseconds(3000);
    std::chrono::milliseconds offTime = std::chrono::milliseconds(500);
    std::chrono::time_point<std::chrono::system_clock> offStartTime;
    std::chrono::time_point<std::chrono::system_clock> fadeInStartTime;
    RGBColor baseColor;
    Ui::SparkleFade *ui;
    std::vector<std::vector<ledDetails>> ledInfo;
    effectState currentEffectState = off;
    std::mt19937 generator; // mersenne_twister_engine seeded with rd()
    bool sparklesSet = false;

    void UpdateColors(std::vector<ControllerZone*> controller_zones);
    void SetDynamicStrings();
    void ResetSparkleFade(std::vector<ControllerZone*> controller_zones);
};
