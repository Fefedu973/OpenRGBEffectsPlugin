/*---------------------------------------------------------*\
| RandomMarquee.h                                           |
|                                                           |
|   OpenRGB Effects Plugin Random Marquee Effect            |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_RandomMarquee.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "hsv.h"

namespace Ui {
class RandomMarquee;
}

class RandomMarquee : public RGBEffect
{
    Q_OBJECT

public:
    explicit RandomMarquee(QWidget *parent = nullptr);
    ~RandomMarquee();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_RANDOM, [](){return new RandomMarquee;});

    static std::string const ClassName() {return "RandomMarquee";}
    static std::string const UI_Name() { return QT_TR_NOOP("Random Marquee"); }

    void StepEffect(std::vector<ControllerZone*>) override;

private slots:
    void changeEvent(QEvent *event) override;

private:
    Ui::RandomMarquee *ui;

    void SetDynamicStrings();

    RGBColor GetColor(unsigned int);
    double custom_rand(double min, double max);

    double progress = 0.0;
    unsigned int last_progress = 0;
    hsv_t random;
    unsigned int spacing = 1;
    double speed_mult = 0.5;
    double progress_mult = 0.5;
    bool dir = false;
};
