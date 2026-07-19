/*---------------------------------------------------------*\
| GifPlayer.h                                               |
|                                                           |
|   OpenRGB Effects Plugin GIF Player Effect                |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include <QMovie>

#include "ui_GifPlayer.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"


namespace Ui {
class GifPlayer;
}

class GifPlayer: public RGBEffect
{
    Q_OBJECT

public:
    explicit GifPlayer(QWidget *parent = nullptr);
    ~GifPlayer();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SPECIAL, [](){return new GifPlayer;});

    static std::string const ClassName() {return "GIFPlayer";}
    static std::string const UI_Name() { return QT_TR_NOOP("Gif Player"); }

    void StepEffect(std::vector<ControllerZone*>) override;

    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

private:
    Ui::GifPlayer   *ui;

    void SetDynamicStrings();

    QMovie *movie;
    QString fileName;

    void Load(QString);

private slots:
    void changeEvent(QEvent *event) override;
    void on_choose_gif_file_clicked();
};
