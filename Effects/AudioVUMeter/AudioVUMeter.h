/*---------------------------------------------------------*\
| AudioVUMeter.h                                            |
|                                                           |
|   OpenRGB Effects Plugin Audio VU Meter Effect            |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "AudioSignalProcessor.h"
#include "AudioSettings.h"
#include "AudioSettingsStruct.h"

#include <QWidget>
#include "ui_AudioVUMeter.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

namespace Ui {
class AudioVUMeter;
}

class AudioVUMeter : public RGBEffect
{
    Q_OBJECT

public:
    explicit AudioVUMeter(QWidget *parent = nullptr);
    ~AudioVUMeter();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_AUDIO, [](){return new AudioVUMeter;});

    static std::string const ClassName() {return "AudioVUMeter";}
    static std::string const UI_Name() { return QT_TR_NOOP("Audio VU Meter"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void EffectState(bool) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_color_offset_valueChanged(int);
    void on_color_spread_valueChanged(int);
    void on_saturation_valueChanged(int);
    void on_invert_hue_stateChanged(int);

    void on_audio_settings_clicked();
    void OnAudioDeviceChanged(int);

private:
    Ui::AudioVUMeter *ui;

    void SetDynamicStrings();

    float   last_height = 0.f;
    float   color_offset = 180.f;
    float   color_spread = 50.f;
    bool    invert_hue = false;
    int     saturation   = 255;

    AudioSettings                   audio_settings;
    Audio::AudioSettingsStruct      audio_settings_struct;
    AudioSignalProcessor            audio_signal_processor;

    void Start();
    void Stop();

    RGBColor GetColor(float, float, float);
};
