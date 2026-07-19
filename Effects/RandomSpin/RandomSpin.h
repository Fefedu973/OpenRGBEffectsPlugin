/*---------------------------------------------------------*\
| RandomSpin.h                                              |
|                                                           |
|   OpenRGB Effects Plugin Random Spin Effect               |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_RandomSpin.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

namespace Ui {
class RandomSpin;
}

struct RandomSpinEntry
{
    double  stop_progress   = 0;
    double  progress        = 0;
    double  speed_mult      = 1.f;
    bool    dir             = true;
    bool    stop            = true;
    double  next_time_point = 0.f;
};

class RandomSpin : public RGBEffect
{
    Q_OBJECT

public:
    explicit RandomSpin(QWidget *parent = nullptr);
    ~RandomSpin();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_RANDOM, [](){return new RandomSpin;});

    static std::string const ClassName() {return "RandomSpin";}
    static std::string const UI_Name() { return QT_TR_NOOP("Random Spin"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void OnControllerZonesListChanged(std::vector<ControllerZone*>)  override;
    void SetUserColors(std::vector<RGBColor> colors) override;

private slots:
    void changeEvent(QEvent *event) override;

private:
    Ui::RandomSpin *ui;

    void SetDynamicStrings();

    RGBColor GetColor(unsigned int i, unsigned int w, const RandomSpinEntry& entry);
    double custom_rand(double, double);

    double progress = 0.0;
    std::vector<RandomSpinEntry> spin_entries;
    QImage gradient;
    void GenerateGradient();
};
