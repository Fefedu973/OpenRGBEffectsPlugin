/*---------------------------------------------------------*\
| GifPlayer.cpp                                             |
|                                                           |
|   OpenRGB Effects Plugin GIF Player Effect                |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "GifPlayer.h"
#include "ColorUtils.h"
#include <QFileDialog>

REGISTER_EFFECT(GifPlayer);

GifPlayer::GifPlayer(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::GifPlayer),
    movie(nullptr)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName = ClassName();
    EffectDetails.HasCustomSettings = true;
    EffectDetails.SupportsRandom = false;
}

GifPlayer::~GifPlayer()
{
    if(movie)
    {
        movie->stop();
        delete movie;
    }

    delete ui;
}

void GifPlayer::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void GifPlayer::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("Use GIFs to create your own effect").toStdString();
}

void GifPlayer::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    if(!movie)
    {
        return;
    }

    QImage image = movie->currentImage();

    for(ControllerZone* controller_zone : controller_zones)
    {
        zone_type ZT = controller_zone->type();

        if((ZT == ZONE_TYPE_SINGLE)
        || (ZT == ZONE_TYPE_LINEAR)
        || (ZT == ZONE_TYPE_LINEAR_LOOP))
        {
            unsigned int width = controller_zone->leds_count();
            unsigned int height = 1;


            QImage scaled = image.scaled(width, height);

            for(unsigned int i = 0; i < width; i++)
            {
                QColor color = scaled.pixelColor(i, 0);
                controller_zone->SetLED(i, ColorUtils::fromQColor(color), Brightness, Temperature, Tint);
            }

        }
        else if((ZT == ZONE_TYPE_MATRIX)
             || (ZT == ZONE_TYPE_MATRIX_LOOP_X)
             || (ZT == ZONE_TYPE_MATRIX_LOOP_Y))
        {
            unsigned int width = controller_zone->matrix_map_width();
            unsigned int height = controller_zone->matrix_map_height();

            QImage scaled = image.scaled(width, height);

            for(unsigned int h = 0; h < height; h++)
            {
                for(unsigned int w = 0; w <  width; w++)
                {
                    QColor color = scaled.pixelColor(w, h);

                    unsigned int led_num = controller_zone->map()[h * width + w];
                    controller_zone->SetLED(led_num, ColorUtils::fromQColor(color), Brightness, Temperature, Tint);
                }
            }
        }
    }
}

void GifPlayer::Load(QString file)
{
    if(!file.isEmpty())
    {
        fileName = file;
        movie = new QMovie(fileName);
        ui->movie->setMovie(movie);
        movie->start();
    }
}

void GifPlayer::on_choose_gif_file_clicked()
{
    Load(QFileDialog::getOpenFileName(this, tr("Open GIF file"), "", tr("GIF Files (*.gif)")));
}

void GifPlayer::LoadCustomSettings(json Settings)
{
    if (Settings.contains("fileName"))
    {
        Load(QString::fromStdString(Settings["fileName"]));
    }
}

json GifPlayer::SaveCustomSettings()
{
    json settings;

    settings["fileName"] = fileName.toStdString();

    return settings;
}
