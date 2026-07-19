/*---------------------------------------------------------*\
| Swap.h                                                    |
|                                                           |
|   OpenRGB Effects Plugin Swap Effect                      |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_Swap.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

namespace Ui {
class Swap;
}

class Swap : public RGBEffect
{
    Q_OBJECT

public:
    explicit Swap(QWidget *parent = nullptr);
    ~Swap();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SIMPLE, [](){return new Swap;});

    static std::string const ClassName() {return "Swap";}
    static std::string const UI_Name() { return QT_TR_NOOP("Swap"); }

    void StepEffect(std::vector<ControllerZone*>) override;

private:
    Ui::Swap *ui;

    void SetDynamicStrings();

    RGBColor GetColor(unsigned int, unsigned int);

    RGBColor c1;
    RGBColor c2;

    RGBColor random1;
    RGBColor random2;

    double time = 0.0;
    double progress = 0.0;
    bool dir = 0;
    bool old_dir = 0;

private slots:
    void changeEvent(QEvent *event) override;
};
