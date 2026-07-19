/*---------------------------------------------------------*\
| BouncingBall.cpp                                          |
|                                                           |
|   OpenRGB Effects Plugin Bouncing Ball Effect             |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "BouncingBall.h"

REGISTER_EFFECT(BouncingBall);

BouncingBall::BouncingBall(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::BouncingBall)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName = ClassName();
    EffectDetails.HasCustomSettings = true;

    SetDefaults();
}

BouncingBall::~BouncingBall()
{
    delete ui;

    for (auto s : bouncingBallSimulations)
    {
        delete s;
    }

    bouncingBallSimulations.clear();
}

void BouncingBall::SetDefaults()
{
    ui->ball_radius_slider->setValue(15);
    ui->drop_height_percent_slider->setValue(90);
    ui->gravity_slider->setValue(10);
    ui->horizontal_velocity_slider->setValue(10);
    ui->spectrum_velocity_slider->setValue(10);
}

void BouncingBall::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void BouncingBall::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("A ball bounces around your RGB setup").toStdString();
}

void BouncingBall::StepEffect(std::vector<ControllerZone*> controllerZones)
{
    for (unsigned int i = 0; i < controllerZones.size(); i++)
    {
        bouncingBallSimulations[i]->DetectSizesChanges();
        bouncingBallSimulations[i]->StepEffect();
    }
}

// Override, in order to re-calculate time delta
void BouncingBall::SetFPS(unsigned int value)
{
    FPS = value;

    for (BouncingBallSimulation* b: bouncingBallSimulations)
    {
        b->SetFps(value);
    }
}

void BouncingBall::SetTemperature(int value)
{
    Temperature = value;

    for (BouncingBallSimulation* b: bouncingBallSimulations)
    {
        b->SetTemperature(value);
    }
}

void BouncingBall::SetTint(int value)
{
    Tint = value;

    for (BouncingBallSimulation* b: bouncingBallSimulations)
    {
        b->SetTint(value);
    }
}

void BouncingBall::SetBrightness(unsigned int value)
{
    Brightness = value;

    for (BouncingBallSimulation* b: bouncingBallSimulations)
    {
        b->SetBrightness(value);
    }
}

void BouncingBall::OnControllerZonesListChanged(std::vector<ControllerZone*> controller_zones)
{
    for (auto s : bouncingBallSimulations)
    {
        delete s;
    }

    bouncingBallSimulations.clear();

    for (unsigned int i = 0; i < controller_zones.size(); i++)
    {
        bouncingBallSimulations.push_back(
            new BouncingBallSimulation(
                controller_zones[i],
                FPS,
                radius,
                gravity,
                horizontalVelocity,
                spectrumVelocity,
                dropHeightPercent,
                Brightness,
                Temperature,
                Tint
            )
        );
    }
}

void BouncingBall::LoadCustomSettings(json Settings)
{
    if(Settings.contains("radius"))                 ui->ball_radius_slider->setValue( Settings["radius"]);
    if(Settings.contains("gravity"))                ui->gravity_slider->setValue(Settings["gravity"]);
    if(Settings.contains("horizontalVelocity"))     ui->horizontal_velocity_slider->setValue(Settings["horizontalVelocity"]);
    if(Settings.contains("spectrumVelocity"))       ui->spectrum_velocity_slider->setValue(Settings["spectrumVelocity"]);
    if(Settings.contains("dropHeightPercent"))      ui->drop_height_percent_slider->setValue(Settings["dropHeightPercent"]);
}

json BouncingBall::SaveCustomSettings()
{
    json settings;

    settings["radius"] = radius;
    settings["gravity"] = gravity;
    settings["horizontalVelocity"] = horizontalVelocity;
    settings["spectrumVelocity"] = spectrumVelocity;
    settings["dropHeightPercent"] = dropHeightPercent;

    return settings;
}

void BouncingBall::on_ball_radius_slider_valueChanged(int value)
{
    radius = value;
    ui->ball_radius_value_label->setText(QString::number(value));

    for (BouncingBallSimulation* b: bouncingBallSimulations)
    {
        b->SetRadius(value);
    }
}

void BouncingBall::on_gravity_slider_valueChanged(int value)
{
    // Update label with gravity value simulation will use
    gravity = BouncingBallSimulation::GetGravity(value);
    ui->gravity_value_label->setText(QString::number(gravity, 'f', 2));

    for (BouncingBallSimulation* b: bouncingBallSimulations)
    {
        b->SetGravity(value);
    }
}

void BouncingBall::on_horizontal_velocity_slider_valueChanged(int value)
{
    horizontalVelocity = value;
    ui->horizontal_velocity_value_label->setText(QString::number(value));

    for (BouncingBallSimulation* b: bouncingBallSimulations)
    {
        b->SetHorizontalVelocity(value);
    }
}

void BouncingBall::on_spectrum_velocity_slider_valueChanged(int value)
{
    spectrumVelocity = value;
    ui->spectrum_velocity_value_label->setText(QString::number(value));

    for (BouncingBallSimulation* b: bouncingBallSimulations)
    {
        b->SetSpectrumVelocity(value);
    }
}

void BouncingBall::on_drop_height_percent_slider_valueChanged(int value)
{
    dropHeightPercent = value;
    ui->drop_height_percent_value_label->setText(QString::number(value));

    for (BouncingBallSimulation* b: bouncingBallSimulations)
    {
        b->SetDropHeightPercent(value);
    }
}

void BouncingBall::on_reset_defaults_button_clicked()
{
    SetDefaults();
}
