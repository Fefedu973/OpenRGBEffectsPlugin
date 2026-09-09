/*---------------------------------------------------------*\
| NoiseMap.cpp                                              |
|                                                           |
|   OpenRGB Effects Plugin Noise Map Effect                 |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "NoiseMap.h"
#include "hsv.h"
#include "ColorUtils.h"
#include <QPainter>
#include <QGradient>

REGISTER_EFFECT(NoiseMap);

NoiseMap::NoiseMap(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::NoiseMap)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.MaxSpeed          = 100;
    EffectDetails.MinSpeed          = 1;
    EffectDetails.HasCustomSettings = true;

    ui->colorsPicker->hide();


    ui->preset_choice->blockSignals(false);
    ui->preset_choice->hide();
    ui->preset_label->hide();

    ui->preset_choice->setCurrentIndex(0);

    SetSpeed(50);
    Defaults();
    ResetNoise();
}

NoiseMap::~NoiseMap()
{
    delete noise;
    delete ui;
}

void NoiseMap::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void NoiseMap::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("Floor is lava").toStdString();

    ui->preset_choice->clear();
    for(const NoiseMapPreset& preset: presets)
    {
        ui->preset_choice->addItem(tr(preset.name));
    }

    ui->colors_choice->clear();
    ui->colors_choice->addItems({tr("Rainbow"),
                                 tr("Inverse rainbow"),
                                 tr("Custom")});
    ui->motion->clear();
    ui->motion->addItems({tr("Up"),
                          tr("Down"),
                          tr("Left"),
                          tr("Right")});
}

void NoiseMap::GenerateGradient()
{
    QGradientStops stops;
    std::vector<RGBColor> colors = ui->colorsPicker->Colors();

    unsigned int colors_count = colors.size();

    float color_step = 1.f / colors_count;

    for(unsigned int i = 0; i < colors_count; i++)
    {
        QGradientStop stop = QGradientStop(
                    i * color_step,
                    QColor(RGBGetRValue(colors[i]), RGBGetGValue(colors[i]), RGBGetBValue(colors[i]))
                    );

        stops << stop;
    }

    QGradient::Spread spread = QGradient::PadSpread;

    QLinearGradient grad(QPointF(0,0), QPointF(100,0));
    grad.setSpread(spread);
    grad.setStops(stops);

    QBrush brush = QBrush(grad);
    QRectF rect(0, 0, 100, 1);
    QPainter painter(&image);
    painter.fillRect(rect, brush);
}

void NoiseMap::on_colorsPicker_ColorsChanged()
{
    GenerateGradient();
}

void NoiseMap::Defaults()
{
    ui->amplitude->setValue(default_amplitude * val_mult);
    ui->frequency->setValue(default_frequency * val_mult);
    ui->lacunarity->setValue(default_lacunarity * val_mult);
    ui->persistence->setValue(default_persistence * val_mult);
    ui->octaves->setValue(default_octaves);
}

void NoiseMap::ResetNoise()
{
    noise = new SimplexNoise(frequency, amplitude, lacunarity, persistence);
}

void NoiseMap::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    for(ControllerZone* controller_zone: controller_zones)
    {
        zone_type ZT = controller_zone->type();
        int leds_count = controller_zone->leds_count();

        if((ZT == ZONE_TYPE_SINGLE)
        || (ZT == ZONE_TYPE_LINEAR)
        || (ZT == ZONE_TYPE_LINEAR_LOOP))
        {
            for(int LedID = 0; LedID < leds_count; LedID++)
            {
                RGBColor color = GetColor(LedID, 0);
                controller_zone->SetLED(LedID, color, Brightness, Temperature, Tint);
            }
        }

        else if((ZT == ZONE_TYPE_MATRIX)
             || (ZT == ZONE_TYPE_MATRIX_LOOP_X)
             || (ZT == ZONE_TYPE_MATRIX_LOOP_Y))
        {
            int cols = controller_zone->matrix_map_width();
            int rows = controller_zone->matrix_map_height();

            for(int col_id = 0; col_id < cols; col_id++)
            {
                for(int row_id = 0; row_id < rows; row_id++)
                {
                    RGBColor color = GetColor(col_id, row_id);
                    int LedID = controller_zone->map()[((row_id * cols) + col_id)];
                    controller_zone->SetLED(LedID, color, Brightness, Temperature, Tint);
                }
            }
        }
    }

    progress +=  0.1 * (float) Speed / (float) FPS;
}

RGBColor NoiseMap::GetColor(unsigned int x, unsigned int y)
{
    float x_shift = 0;
    float y_shift = 0;

    switch (motion) {

    case 0: y_shift = motion_speed * progress; break;
    case 1: y_shift = motion_speed * -progress;  break;
    case 2: x_shift = motion_speed * progress;  break;
    case 3: x_shift = motion_speed * -progress;  break;

    default: break;
    }


    float value = noise->fractal(octaves, x + x_shift, y + y_shift, progress);
    float frac = (1+value)*0.5;

    int color_choice = ui->colors_choice->currentIndex();
    hsv_t hsv;

    switch (color_choice)
    {
    case 0:
        hsv.hue = 360 * frac;
        hsv.saturation = 255;
        hsv.value = 255 ;
        return RGBColor(hsv2rgb(&hsv));

    case 1:
        hsv.hue = 360 - 360 * frac;
        hsv.saturation = 255;
        hsv.value = 255 ;
        return RGBColor(hsv2rgb(&hsv));

    case 2:
        int color_x = (1 - frac) * 100;
        QColor c = image.pixelColor(color_x, 0);
        return ColorUtils::fromQColor(c);
    }

    return ColorUtils::OFF();
}

void NoiseMap::LoadCustomSettings(json settings)
{
    if(settings.contains("frequency"))  frequency  = settings["frequency"];
    if(settings.contains("amplitude"))  amplitude  = settings["amplitude"];
    if(settings.contains("lacunarity"))  lacunarity  = settings["lacunarity"];
    if(settings.contains("persistence"))  persistence  = settings["persistence"];
    if(settings.contains("octaves"))  octaves  = settings["octaves"];
    if(settings.contains("motion"))   motion  = settings["motion"];
    if(settings.contains("motion_speed"))  motion_speed  = settings["motion_speed"];

    ui->amplitude->setValue(amplitude * val_mult);
    ui->frequency->setValue(frequency * val_mult);
    ui->lacunarity->setValue(lacunarity * val_mult);{








    }
    ui->persistence->setValue(persistence * val_mult);
    ui->octaves->setValue(octaves);
    ui->motion_speed->setValue(motion_speed);
    ui->motion->setCurrentIndex(motion);


    if (settings.contains("colors_choice"))
    {
        int colors_choice = settings["colors_choice"];
        ui->colors_choice->setCurrentIndex(colors_choice);
    }

    if (settings.contains("colors"))
    {
        ui->colorsPicker->SetColors(settings["colors"]);
    }
}

json NoiseMap::SaveCustomSettings()
{
    json settings;

    settings["frequency"]       = frequency;
    settings["amplitude"]       = amplitude;
    settings["lacunarity"]      = lacunarity;
    settings["persistence"]     = persistence;
    settings["octaves"]         = octaves;
    settings["colors"]          = ui->colorsPicker->Colors();
    settings["colors_choice"]   = ui->colors_choice->currentIndex();
    settings["motion"]          = motion;
    settings["motion_speed"]    = motion_speed;

    return settings;
}

void NoiseMap::on_amplitude_valueChanged(int value)
{
    amplitude = value * inv_val_mult;
    ResetNoise();
}

void NoiseMap::on_frequency_valueChanged(int value)
{
    frequency = value * inv_val_mult;
    ResetNoise();
}

void NoiseMap::on_lacunarity_valueChanged(int value)
{
    lacunarity = value * inv_val_mult;
    ResetNoise();
}

void NoiseMap::on_persistence_valueChanged(int value)
{
    persistence = value * inv_val_mult;
    ResetNoise();
}

void NoiseMap::on_octaves_valueChanged(int value)
{
    octaves = value;
    ResetNoise();
}

void NoiseMap::on_motion_speed_valueChanged(int value)
{
    motion_speed = value;
}

void NoiseMap::on_motion_currentIndexChanged(int value)
{
    motion = value;
}

void NoiseMap::on_defaults_clicked()
{
    Defaults();
}

void NoiseMap::on_colors_choice_currentIndexChanged(int value)
{
    ui->colorsPicker->setVisible(value==2);
    ui->preset_choice->setVisible(value==2);
    ui->preset_label->setVisible(value==2);
}

void NoiseMap::on_preset_choice_currentTextChanged(int value)
{
    if(value > -1)
    {
        ui->colorsPicker->SetColors(presets[value].colors);
    }
}
