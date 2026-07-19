/*---------------------------------------------------------*\
| AudioStar.h                                               |
|                                                           |
|   OpenRGB Effects Plugin Audio Star Effect                |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "AudioSignalProcessor.h"
#include "AudioSettings.h"
#include "AudioSettingsStruct.h"

#include <QWidget>
#include "ui_AudioStar.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

namespace Ui {
class AudioStar;
}

class AudioStar : public RGBEffect
{
    Q_OBJECT

public:
    explicit AudioStar(QWidget *parent = nullptr);
    ~AudioStar();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_AUDIO, [](){return new AudioStar;});

    static std::string const ClassName() {return "AudioStar";}
    static std::string const UI_Name() { return QT_TR_NOOP("Audio Star"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void EffectState(bool) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void OnAudioDeviceChanged(int);

    void on_audio_settings_clicked();
    void on_edge_beat_stateChanged(int);
    void on_edge_beat_sensivity_valueChanged(int);
    void on_edge_beat_hue_valueChanged(int);
    void on_edge_beat_saturation_valueChanged(int);

private:
    Ui::AudioStar *ui;

    void SetDynamicStrings();

    bool    edge_beat = false;
    int     edge_beat_sensivity  = 100;
    int     edge_beat_saturation  = 0;
    int     edge_beat_hue  = 0;
    double  time = 0;
    double  amp;
    float   side = 1.f;

    void Start();
    void Stop();
    RGBColor GetColor(float, float, float, float);    

    AudioSettings                   audio_settings;
    Audio::AudioSettingsStruct      audio_settings_struct;
    AudioSignalProcessor            audio_signal_processor;
};
