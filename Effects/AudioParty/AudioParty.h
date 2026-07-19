/*---------------------------------------------------------*\
| AudioParty.h                                              |
|                                                           |
|   OpenRGB Effects Plugin Audio Party Effect               |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "AudioSignalProcessor.h"
#include "AudioSettings.h"
#include "AudioSettingsStruct.h"

#include <QWidget>
#include "ui_AudioParty.h"
#include "ColorUtils.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "ctkrangeslider.h"

namespace Ui {
class AudioParty;
}

class AudioParty : public RGBEffect
{
    Q_OBJECT

public:
    explicit AudioParty(QWidget *parent = nullptr);
    ~AudioParty();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_AUDIO, [](){return new AudioParty;});

    static std::string const ClassName() {return "AudioParty";}
    static std::string const UI_Name() { return QT_TR_NOOP("Audio Party"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void EffectState(bool) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void OnAudioDeviceChanged(int);
    void on_audio_settings_clicked();

    void on_divisions_valueChanged(int);
    void on_effect_threshold_valueChanged(double);
    void ZonesChanged(int, int);
    void UpdateGraph(QPixmap);

signals:
    void UpdateGraphSignal(QPixmap) const;

private:
    Ui::AudioParty *ui;

    void SetDynamicStrings();
    void Start();
    void Stop();

    double x_shift = 0.f;
    double color_shift = 0.f;
    float divisions = 2.0;
    float effect_threshold = 0.2;
    unsigned int motion_zone_stop = 64;
    unsigned int color_zone_stop = 192;

    RGBColor background = ColorUtils::OFF();
    RGBColor wave_color = ToRGBColor(0,255,0);

    RGBColor GetColor(float, float, float, float);

    RGBColor color;

    double effect_progress = 1.0;
    unsigned int effect_idx = 0;

    ctkRangeSlider* zones_slider;

    AudioSettings                   audio_settings;
    Audio::AudioSettingsStruct      audio_settings_struct;
    AudioSignalProcessor            audio_signal_processor;
};
