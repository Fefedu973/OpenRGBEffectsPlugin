/*---------------------------------------------------------*\
| Bloom.h                                                   |
|                                                           |
|   OpenRGB Effects Plugin Bloom Effect                     |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_Bloom.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "hsv.h"

namespace Ui {
class Bloom;
}

struct Flower {
    hsv_t hsv;
    float hue;
    int saturation;
    float speed_mult = 1.f;
};

class Bloom : public RGBEffect
{
    Q_OBJECT

public:
    explicit Bloom(QWidget *parent = nullptr);
    ~Bloom();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_RANDOM, [](){return new Bloom;});

    static std::string const ClassName() {return "Bloom";}
    static std::string const UI_Name() { return QT_TR_NOOP("Bloom"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void OnControllerZonesListChanged(std::vector<ControllerZone*>) override;

    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
	void changeEvent(QEvent *event) override;
    void on_saturation_valueChanged(int);

private:
    Ui::Bloom *ui;

    void SetDynamicStrings();

    std::vector<std::vector<Flower>> flowers;

    void Reset(std::vector<ControllerZone*>);

    void UpdateFlowers(unsigned int);
    int  saturation   = 255;
};
