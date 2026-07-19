/*---------------------------------------------------------*\
| CustomMarquee.h                                           |
|                                                           |
|   OpenRGB Effects Plugin Custom Marquee Effect            |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_CustomMarquee.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

namespace Ui {
class CustomMarquee;
}

class CustomMarquee : public RGBEffect
{
    Q_OBJECT

public:
    explicit CustomMarquee(QWidget *parent = nullptr);
    ~CustomMarquee();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_ADVANCED, [](){return new CustomMarquee;});

    static std::string const ClassName() {return "CustomMarquee";}
    static std::string const UI_Name() { return QT_TR_NOOP("Custom Marquee"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private:
    Ui::CustomMarquee *ui;

    void SetDynamicStrings();

    RGBColor GetColor(unsigned int);
    double progress = 0.0;

private slots:
    void changeEvent(QEvent *event) override;
};
