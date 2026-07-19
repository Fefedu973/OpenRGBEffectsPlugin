/*---------------------------------------------------------*\
| Marquee.h                                                 |
|                                                           |
|   OpenRGB Effects Plugin Marquee Effect                   |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_Marquee.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "hsv.h"

namespace Ui {
class Marquee;
}

class Marquee : public RGBEffect
{
    Q_OBJECT

public:
    explicit Marquee(QWidget *parent = nullptr);
    ~Marquee();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SIMPLE, [](){return new Marquee;});

    static std::string const ClassName() {return "Marquee";}
    static std::string const UI_Name() { return QT_TR_NOOP("Marquee"); }

    void StepEffect(std::vector<ControllerZone*>) override;

private:
    Ui::Marquee *ui;

    void SetDynamicStrings();

    RGBColor GetColor(unsigned int);
    double progress = 0.0;

    hsv_t random;

private slots:
    void changeEvent(QEvent *event) override;
};
