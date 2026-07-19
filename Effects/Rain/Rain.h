/*---------------------------------------------------------*\
| Rain.h                                                    |
|                                                           |
|   OpenRGB Effects Plugin Rain Effect                      |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include <QWidget>
#include "ui_Rain.h"

namespace Ui {
class Rain;
}

struct Drop
{
    double progress;
    RGBColor color;
    unsigned int col;
    float speed_mult;
    unsigned int size;
};

class Rain : public RGBEffect
{
    Q_OBJECT

public:
    explicit Rain(QWidget *parent = nullptr);
    ~Rain();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_ADVANCED, [](){return new Rain;});

    static std::string const ClassName() { return "Rain"; }
    static std::string const UI_Name() { return QT_TR_NOOP("Rain"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void OnControllerZonesListChanged(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_size_valueChanged(int);

private:
    Ui::Rain *ui;

    void SetDynamicStrings();

    int size = 1;

    RGBColor GetColor(unsigned int, unsigned int, unsigned int);
    void  TriggerDrop(unsigned int, unsigned int);
    void  CleanDrops(unsigned int, unsigned int);
    void  RunDrops(unsigned int);

    std::vector<std::vector<Drop>> drops;
};
