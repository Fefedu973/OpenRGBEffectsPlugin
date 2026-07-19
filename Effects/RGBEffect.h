/*---------------------------------------------------------*\
| RGBEffect.h                                               |
|                                                           |
|   OpenRGB Effects Plugin RGB Effect Base Class            |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include <QLayout>
#include <nlohmann/json.hpp>
#include "ControllerZone.h"

#define CAT_ADVANCED    QT_TRANSLATE_NOOP("RGBEffect", "Advanced")
#define CAT_AUDIO       QT_TRANSLATE_NOOP("RGBEffect", "Audio")
#define CAT_BEAMS       QT_TRANSLATE_NOOP("RGBEffect", "Beams")
#define CAT_RAINBOW     QT_TRANSLATE_NOOP("RGBEffect", "Rainbow")
#define CAT_RANDOM      QT_TRANSLATE_NOOP("RGBEffect", "Random")
#define CAT_SIMPLE      QT_TRANSLATE_NOOP("RGBEffect", "Simple")
#define CAT_SPECIAL     QT_TRANSLATE_NOOP("RGBEffect", "Special")

using json = nlohmann::json;

struct EffectInfo
{
    std::string EffectName = "";
    std::string EffectClassName = "";
    std::string EffectDescription = "";
    std::string CustomName = "";

    bool IsReversable = false;
    bool SupportsRandom = true;
    int  MaxSpeed = 0;
    int  MinSpeed = 0;

    bool AllowOnlyFirst = false;
    unsigned int  UserColors = 0;

    int         MaxSlider2Val = 0;
    int         MinSlider2Val = 0;
    std::string Slider2Name = "";

    bool HasCustomSettings = false;
    bool ExpandCustomSettings = false;
};

class RGBEffect : public QWidget
{
    Q_OBJECT

public:
    explicit RGBEffect(QWidget* parent = nullptr) : QWidget(parent){};
    virtual ~RGBEffect(){};

    // todo: rename this method
    // => LoadCustomLayout
    virtual void StepEffect(std::vector<ControllerZone*>) {};
    virtual void OnControllerZonesListChanged(std::vector<ControllerZone*>) {}

    virtual void EffectState(bool Enabled) { EffectEnabled = Enabled; }
    virtual bool IsEnabled() { return EffectEnabled; }

    virtual void LoadCustomSettings(json) {}
    virtual json SaveCustomSettings() { return json(); }

    virtual void SetAutoStart(bool value) { AutoStart = value;}
    virtual bool IsAutoStart() { return AutoStart;}

    virtual void SetFPS(unsigned int value) { FPS = value; }
    virtual unsigned int GetFPS()  { return FPS; }

    virtual void SetSpeed(unsigned int value) { Speed = value; }
    virtual unsigned int GetSpeed() { return Speed; };

    virtual void SetUserColors(std::vector<RGBColor> colors) { UserColors = colors; }
    virtual std::vector<RGBColor> GetUserColors() {return UserColors; }

    virtual void SetRandomColorsEnabled(bool value) { RandomColorsEnabled = value; }
    virtual bool IsRandomColorsEnabled() { return RandomColorsEnabled; }

    virtual void SetOnlyFirstColorEnabled(bool value) {OnlyFirstColorEnabled = value; }
    virtual bool IsOnlyFirstColorEnabled() { return OnlyFirstColorEnabled; }

    virtual unsigned int GetSlider2Val() { return Slider2Val; }
    virtual void SetSlider2Val(unsigned int value) { Slider2Val = value; }

    virtual unsigned int GetBrightness() { return Brightness; }
    virtual void SetBrightness(unsigned int value) { Brightness = value; }

    virtual int GetTemperature() { return Temperature; }
    virtual void SetTemperature(int value) { Temperature = value; }

    virtual int GetTint() { return Tint; }
    virtual void SetTint(int value) { Tint = value; }

    EffectInfo EffectDetails;

    void EmitMeasure(float t, int d){ emit TimeMeasured(t, d); }

    json ToJson()
    {
        json j;

        j["EffectClassName"]  = EffectDetails.EffectClassName;
        j["CustomName"]       = EffectDetails.CustomName;
        j["FPS"]              = GetFPS();
        j["Speed"]            = GetSpeed();
        j["Slider2Val"]       = GetSlider2Val();
        j["RandomColors"]     = IsRandomColorsEnabled();
        j["AllowOnlyFirst"]   = IsOnlyFirstColorEnabled();
        j["Brightness"]       = GetBrightness();
        j["Temperature"]      = GetTemperature();
        j["Tint"]             = GetTint();

        std::vector<RGBColor> colors = GetUserColors();

        for (unsigned int c = 0; c < colors.size(); c++)
        {
            j["UserColors"][c] = colors[c];
        }

        if (EffectDetails.HasCustomSettings)
        {
            j["CustomSettings"] = SaveCustomSettings();
        }

        return j;
    }

signals:
    void TimeMeasured(float, int);

protected:
    unsigned int FPS = 60;
    unsigned int Speed = 1;
    unsigned int Slider2Val = 1;
    std::vector<RGBColor> UserColors;
    bool RandomColorsEnabled = false;
    bool AutoStart = false;
    bool OnlyFirstColorEnabled = false;
    bool EffectEnabled = false;
    float Brightness = 100;
    int Temperature = 0;
    int Tint = 0;
};
