/*---------------------------------------------------------*\
| SpectrumCycling.h                                         |
|                                                           |
|   OpenRGB Effects Plugin Spectrum Cycling Effect          |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include <QWidget>

namespace Ui {
class SpectrumCycling;
}
class SpectrumCycling : public RGBEffect
{
    Q_OBJECT

public:
    explicit SpectrumCycling(QWidget *parent = nullptr);
    ~SpectrumCycling();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_RAINBOW, [](){return new SpectrumCycling;});

    static std::string const ClassName() {return "SpectrumCycling";}
    static std::string const UI_Name() { return QT_TR_NOOP("Spectrum Cycling"); }

    void StepEffect(std::vector<ControllerZone*>) override;

    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_saturation_valueChanged(int);

private:
    Ui::SpectrumCycling *ui;

    void SetDynamicStrings();

    double progress = 0.0;
    int  saturation   = 255;
};
