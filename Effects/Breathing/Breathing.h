/*---------------------------------------------------------*\
| Breathing.h                                               |
|                                                           |
|   OpenRGB Effects Plugin Breathing Effect                 |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "hsv.h"
#include "ui_Breathing.h"
#include <QWidget>

namespace Ui {
class Breathing;
}

class Breathing: public RGBEffect
{
    Q_OBJECT

public:
    explicit Breathing(QWidget *parent = nullptr);
    ~Breathing();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SIMPLE, [](){return new Breathing;});

    static std::string const ClassName() { return "Breathing"; }
    static std::string const UI_Name() { return QT_TR_NOOP("Breathing"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_colorsPicker_ColorsChanged();

private:
    Ui::Breathing *ui;

    void SetDynamicStrings();

    float Progress = 3.14159; // This is to ensure that the colorPicker colors are used when calling StepEffect the first time
    hsv_t CurrentColor;
    int colorLoopIndex = 0;
};
