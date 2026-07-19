/*---------------------------------------------------------*\
| RotatingBeam.h                                            |
|                                                           |
|   OpenRGB Effects Plugin Rotating Beam Effect             |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ui_RotatingBeam.h"
#include "hsv.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"

namespace Ui {
class RotatingBeam;
}

class RotatingBeam : public RGBEffect
{
    Q_OBJECT

public:
    explicit RotatingBeam(QWidget *parent = nullptr);
    ~RotatingBeam();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_BEAMS, [](){return new RotatingBeam;});

    static std::string const ClassName() {return "RotatingBeam";}
    static std::string const UI_Name() { return QT_TR_NOOP("Rotating Beam"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void SetUserColors(std::vector<RGBColor> colors) override;
    void SetRandomColorsEnabled(bool value) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_mode_currentIndexChanged(int);
    void on_thickness_valueChanged(int);

private:
    Ui::RotatingBeam *ui;

    void SetDynamicStrings();

    double progress = 0.0;

    int thickness = 0;

    RGBColor GetColor(float, float, QLineF, float, float);

    hsv_t hsv1;
    hsv_t hsv2;

    void ResetUserColors();

    enum RotatingBeamMode
    {
        CLOCKWISE = 0,
        COUNTER_CLOCKWISE = 1,
        PENDULUM = 2,
        WIPERS = 3,
        SWING_H = 4,
        SWING_V = 5
    };

    QStringList modes = QStringList(
                {
                    QT_TR_NOOP("Clockwise"),
                    QT_TR_NOOP("Counter clockwise"),
                    QT_TR_NOOP("Pendulum"),
                    QT_TR_NOOP("Wipers"),
                    QT_TR_NOOP("Swing H"),
                    QT_TR_NOOP("Swing V")
                });

    RotatingBeamMode mode = CLOCKWISE;

    QLineF Rotate();
};
