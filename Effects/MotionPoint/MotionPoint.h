/*---------------------------------------------------------*\
| MotionPoint.h                                             |
|                                                           |
|   OpenRGB Effects Plugin Motion Point Effect              |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_MotionPoint.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "ColorUtils.h"

namespace Ui {
class MotionPoint;
}

class MotionPoint : public RGBEffect
{
    Q_OBJECT

public:
    explicit MotionPoint(QWidget *parent = nullptr);
    ~MotionPoint();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SIMPLE, [](){return new MotionPoint;});

    static std::string const ClassName() {return "MotionPoint";}
    static std::string const UI_Name() { return QT_TR_NOOP("Motion Point"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_background_ColorSelected(QColor);

private:
    Ui::MotionPoint *ui;

    void SetDynamicStrings();

    RGBColor GetColor(unsigned int, unsigned int, double);

    double progress = 0.f;
    RGBColor background = ColorUtils::OFF();
    RGBColor current = ColorUtils::OFF();

};
