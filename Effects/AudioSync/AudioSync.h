/*---------------------------------------------------------*\
| AudioSync.h                                               |
|                                                           |
|   OpenRGB Effects Plugin Audio Sync Effect                |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "AudioSignalProcessor.h"
#include "AudioSettings.h"
#include "AudioSettingsStruct.h"

#include <QLayout>
#include <QComboBox>
#include <QWidget>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSpacerItem>
#include <QTimer>

#include "ui_AudioSync.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

#define SILENT_COLOR_TIMEOUT 120

enum SaturationMode
{
    NO_SATURATION = 0,
    SATURATE_HIGH_AMPLITUDES = 1,
    B_W_MODE = 2
};

enum RollMode
{
    LINEAR = 0,
    NONE = 1,
    RADIAL = 2,
    WAVE = 3,
    LINEAR2 = 4
};

namespace Ui {
    class AudioSync;
}

class AudioSync: public RGBEffect
{
    Q_OBJECT

public:
    explicit AudioSync(QWidget *parent = nullptr);
    ~AudioSync();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_AUDIO, [](){return new AudioSync;});

    static std::string const ClassName() {return "AudioSync";}
    static std::string const UI_Name() { return QT_TR_NOOP("Audio Sync"); }

    void StepEffect(std::vector<ControllerZone*>)               override;
    void LoadCustomSettings(json)                               override;
    json SaveCustomSettings()                                   override;
    void EffectState(bool)                                      override;

private slots:
    void changeEvent(QEvent *event) override;
    void OnAudioDeviceChanged(int);
    void UpdateGraph(QPixmap);

    void on_audio_settings_clicked();
    void on_color_fade_speed_valueChanged(int);
    void on_hue_shift_valueChanged(int);
    void on_bypass_valuesChanged(int,int);
    void on_saturation_currentIndexChanged(int);
    void on_roll_mode_currentIndexChanged(int);
    void on_silent_color_stateChanged(int);
    void on_silent_color_value_ColorSelected(QColor);

signals:
    void UpdateGraphSignal(QPixmap) const;

private:    
    Ui::AudioSync *ui;

    void SetDynamicStrings();

    /*-----*\
    | Ui    |
    \*-----*/
    QFrame* PrimaryFrame;
    QFrame* AudioSyncFrame;

    /*----------*\
    | internals  |
    \*----------*/
    int                     immediate_freq_hue = 0;
    int                     current_freq_hue = 0;
    float                   current_freq_sat = 0;
    float                   current_freq_val = 0;
    std::vector<int>        rainbow_hues;
    std::vector<RGBColor>   colors_rotation;
    int                     fade_step;
    int                     hue_shift;
    int                     bypass_min;
    int                     bypass_max;
    int                     saturation_mode;
    int                     roll_mode;
    bool                    silent_color = false;
    RGBColor                silent_color_value = 0;
    unsigned int            silent_color_timer = 0;

    AudioSettings                   audio_settings;
    Audio::AudioSettingsStruct      audio_settings_struct;
    AudioSignalProcessor            audio_signal_processor;

    /*--------*\
    | Methods  |
    \*--------*/
    void Start();
    void Stop();
    void CreateUi();
    RGBColor GetColor(int, int, int, int);

};
