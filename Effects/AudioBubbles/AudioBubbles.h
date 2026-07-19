/*---------------------------------------------------------*\
| AudioBubbles.h                                            |
|                                                           |
|   OpenRGB Effects Plugin Audio Bubbles Effect             |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "AudioSignalProcessor.h"
#include "AudioSettings.h"
#include "AudioSettingsStruct.h"

#include "ui_AudioBubbles.h"
#include <QWidget>
#include "RGBEffect.h"
#include "EffectRegisterer.h"

typedef struct
{
    const char * name;
    std::vector<RGBColor> colors;

} AudioBubblesPreset;

typedef struct
{
    unsigned int freq_id;
    double amp;
    QPointF center;
    double progress;
    double speed;

} AudioBubble;

enum BubblesCenterSpawnMode
{
    RANDOM_XY = 0,
    RANDOM_X = 1,
    RANDOM_Y = 2,
    CENTER = 3
};

namespace Ui {
class AudioBubbles;
}

class AudioBubbles: public RGBEffect
{
    Q_OBJECT

public:
    explicit AudioBubbles(QWidget *parent = nullptr);
    ~AudioBubbles();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_AUDIO, [](){return new AudioBubbles;});

    static std::string const ClassName() {return "AudioBubbles";}
    static std::string const UI_Name() { return QT_TR_NOOP("Audio Bubbles"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void EffectState(bool) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_preset_currentIndexChanged(int);
    void on_spawnMode_currentIndexChanged(int);
    void on_trigger_valueChanged(int);
    void on_max_bubbles_valueChanged(int);
    void on_max_expansion_valueChanged(int);
    void on_bubbles_thickness_valueChanged(int);
    void on_speed_mult_valueChanged(int);
    void on_audio_settings_clicked();
    void OnAudioDeviceChanged(int);
    void on_colorsPicker_ColorsChanged();
    void UpdateGraph(QPixmap);

signals:
    void UpdateGraphSignal(QPixmap) const;

private:
    Ui::AudioBubbles   *ui;

    void SetDynamicStrings();
    void ExpandBubbles();
    void TriggerBubbles();
    void InitBubble(int, double);
    void Cleanup();
    RGBColor GetColor(int, int, int, int);
    void GenerateGradient();
    void Start();
    void Stop();

    std::vector<AudioBubble> bubbles;

    /*
    std::vector<double>         speeds;
    std::vector<unsigned int>   ids;
    std::vector<double>         bubbles;
    std::vector<RGBColor>       colors;
    std::vector<QPointF>        centers;
    */

    AudioSettings                   audio_settings;
    Audio::AudioSettingsStruct      audio_settings_struct;
    AudioSignalProcessor            audio_signal_processor;


    QImage gradient;

    // todo review defaults    
    BubblesCenterSpawnMode spawnMode = RANDOM_XY;

    unsigned int trigger = 30;           // volume 1 -> 100
    unsigned int max_bubbles = 8;        // global max number
    unsigned int speed_mult = 1;         // global speed mult
    unsigned int max_expansion = 100;    // global max size
    unsigned int bubbles_thickness = 10; // global max ring size

    std::vector<AudioBubblesPreset> presets =
    {
        {QT_TR_NOOP("Unicorn Vomit"), std::vector<RGBColor>{
            HEXCOLOR(0xff0000),
            HEXCOLOR(0xff00e6),
            HEXCOLOR(0x0000ff),
            HEXCOLOR(0x00b3ff),
            HEXCOLOR(0x00ff51),
            HEXCOLOR(0xeaff00),
            HEXCOLOR(0xffb300),
            HEXCOLOR(0xff0000)
        }},

        {QT_TR_NOOP("Borealis"), std::vector<RGBColor>{
            HEXCOLOR(0x14e81e),
            HEXCOLOR(0x00ea8d),
            HEXCOLOR(0x017ed5),
            HEXCOLOR(0xb53dff),
            HEXCOLOR(0x8d00c4),
            HEXCOLOR(0x14e81e),
        }},

        {QT_TR_NOOP("Ocean"), std::vector<RGBColor>{
            HEXCOLOR(0x00007f),
            HEXCOLOR(0x0000ff),
            HEXCOLOR(0x00ffff),
            HEXCOLOR(0x00aaff),
            HEXCOLOR(0x00007f),
        }},

        {QT_TR_NOOP("Pink/Blue"), std::vector<RGBColor>{
            HEXCOLOR(0xfe00c5),
            HEXCOLOR(0x00c5ff),
            HEXCOLOR(0x00c5ff),
            HEXCOLOR(0xfe00c5)
        }},

        {QT_TR_NOOP("Pink/Gold"), std::vector<RGBColor>{
            HEXCOLOR(0xfee000),
            HEXCOLOR(0xfe00fe),
            HEXCOLOR(0xfe00fe),
            HEXCOLOR(0xfee000),
        }},

        {QT_TR_NOOP("Pulse"), std::vector<RGBColor>{
            HEXCOLOR(0xff5500),
            HEXCOLOR(0x000000),
            HEXCOLOR(0x000000),
            HEXCOLOR(0x000000),
            HEXCOLOR(0xff5500),
        }},

        {QT_TR_NOOP("Purple/Orange"), std::vector<RGBColor>{
            HEXCOLOR(0xff2100),
            HEXCOLOR(0xaa00ff),
            HEXCOLOR(0xaa00ff),
            HEXCOLOR(0xff2100),
            HEXCOLOR(0xff2100),
            HEXCOLOR(0xff2100)
        }},

        {QT_TR_NOOP("LightBlue/Purple"), std::vector<RGBColor>{
            HEXCOLOR(0x03fffa),
            HEXCOLOR(0x55007f),
            HEXCOLOR(0x55007f),
            HEXCOLOR(0x03fffa)
        }},

        {QT_TR_NOOP("Police Beacon"), std::vector<RGBColor>{
            HEXCOLOR(0xff0000),
            HEXCOLOR(0x0000ff),
            HEXCOLOR(0x0000ff),
            HEXCOLOR(0xff0000),
            HEXCOLOR(0xff0000)
        }},

        {QT_TR_NOOP("Seabed"), std::vector<RGBColor>{
            HEXCOLOR(0x00ff00),
            HEXCOLOR(0x0032ff),
            HEXCOLOR(0x0032ff),
            HEXCOLOR(0x00ff00),
            HEXCOLOR(0x00ff00)
        }},

        {QT_TR_NOOP("Sunset"), std::vector<RGBColor>{
            HEXCOLOR(0xff2100),
            HEXCOLOR(0xab006d),
            HEXCOLOR(0xc01c52),
            HEXCOLOR(0xd53737),
            HEXCOLOR(0xea531b),
            HEXCOLOR(0xff6e00),
            HEXCOLOR(0xff0000),
            HEXCOLOR(0xff2100)
        }},

        {QT_TR_NOOP("Vaporwave"), std::vector<RGBColor>{
            HEXCOLOR(0xFF71CE),
            HEXCOLOR(0xB967FF),
            HEXCOLOR(0x01CDFE),
            HEXCOLOR(0x05FFA1),
            HEXCOLOR(0xFFFB96),
            HEXCOLOR(0xFF71CE)
        }}
    };
};
