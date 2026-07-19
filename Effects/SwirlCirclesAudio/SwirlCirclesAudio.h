/*---------------------------------------------------------*\
| SwirlCirclesAudio.h                                       |
|                                                           |
|   OpenRGB Effects Plugin Swirl Circles Audio Effect       |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "AudioSignalProcessor.h"
#include "AudioSettings.h"
#include "AudioSettingsStruct.h"

#include <QWidget>
#include "ui_SwirlCirclesAudio.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "hsv.h"

namespace Ui {
class SwirlCirclesAudio;
}

class SwirlCirclesAudio : public RGBEffect
{
    Q_OBJECT

public:
    explicit SwirlCirclesAudio(QWidget *parent = nullptr);
    ~SwirlCirclesAudio();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_AUDIO, [](){return new SwirlCirclesAudio;});

    static std::string const ClassName() {return "SwirlCirclesAudio";}
    static std::string const UI_Name() { return QT_TR_NOOP("Swirl Circles Audio"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void SetUserColors(std::vector<RGBColor> colors) override;
    void SetRandomColorsEnabled(bool value) override;
    void EffectState(bool) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_radius_valueChanged(int);
    void on_audio_settings_clicked();
    void OnAudioDeviceChanged(int);

private:
    Ui::SwirlCirclesAudio *ui;

    void SetDynamicStrings();

    double progress = 0.0;
    hsv_t hsv1;
    hsv_t hsv2;
    int radius = 0;
    float current_level = 0.f;

    AudioSettings                   audio_settings;
    Audio::AudioSettingsStruct      audio_settings_struct;
    AudioSignalProcessor            audio_signal_processor;

    void Start();
    void Stop();    
    void ResetUserColors();
    RGBColor GetColor(unsigned int, unsigned int,  unsigned int,  unsigned int, float, float);
};
