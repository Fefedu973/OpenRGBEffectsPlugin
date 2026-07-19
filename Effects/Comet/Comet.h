/*---------------------------------------------------------*\
| Comet.h                                                   |
|                                                           |
|   OpenRGB Effects Plugin Comet Effect                     |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_Comet.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "hsv.h"

namespace Ui {
class Comet;
}

class Comet : public RGBEffect
{
    Q_OBJECT

public:
    explicit Comet(QWidget *parent = nullptr);
    ~Comet();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SIMPLE, [](){return new Comet;});

    static std::string const ClassName() {return "Comet";}
    static std::string const UI_Name() { return QT_TR_NOOP("Comet"); }

    void StepEffect(std::vector<ControllerZone*>) override;

private:
    Ui::Comet *ui;

    void SetDynamicStrings();

    double time = 0;
    double progress = 0;

    RGBColor GetColor(unsigned int, unsigned int);

    hsv_t tmp;

private slots:
    void changeEvent(QEvent *event) override;
};
