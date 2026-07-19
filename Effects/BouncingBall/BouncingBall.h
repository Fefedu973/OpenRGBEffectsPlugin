/*---------------------------------------------------------*\
| BouncingBall.h                                            |
|                                                           |
|   OpenRGB Effects Plugin Bouncing Ball Effect             |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include <vector>
#include "RGBEffect.h"
#include "BouncingBallSimulation.h"
#include "EffectRegisterer.h"
#include "ui_BouncingBall.h"

namespace Ui {
    class BouncingBall;
}

class BouncingBall: public RGBEffect
{
    Q_OBJECT

public:
    explicit BouncingBall(QWidget *parent = nullptr);
    ~BouncingBall();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_ADVANCED, [](){ return new BouncingBall; });

    static std::string const ClassName() { return "BouncingBall"; }
    static std::string const UI_Name() { return QT_TR_NOOP("Bouncing Ball"); }

    void SetFPS(unsigned int value) override;
    void SetBrightness(unsigned int value) override;
    void SetTemperature(int value) override;
    void SetTint(int value) override;
    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;
    void OnControllerZonesListChanged(std::vector<ControllerZone*>) override;

private:
    Ui::BouncingBall *ui;

    void SetDefaults();
    void SetDynamicStrings();

    std::vector<BouncingBallSimulation*> bouncingBallSimulations;

    int radius;
    int gravity;
    int dropHeightPercent;
    int spectrumVelocity;
    int horizontalVelocity;

private slots:
       void changeEvent(QEvent *event) override;
       void on_ball_radius_slider_valueChanged(int value);
       void on_gravity_slider_valueChanged(int value);
       void on_horizontal_velocity_slider_valueChanged(int value);
       void on_spectrum_velocity_slider_valueChanged(int value);
       void on_drop_height_percent_slider_valueChanged(int value);
       void on_reset_defaults_button_clicked();
};
