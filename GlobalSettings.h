/*---------------------------------------------------------*\
| GlobalSettings.h                                          |
|                                                           |
|   OpenRGB Effects Plugin Global Settings Widget           |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "AudioSettings.h"
#include "ui_GlobalSettings.h"

namespace Ui
{
    class GlobalSettings;
}

class GlobalSettings : public QWidget
{
    Q_OBJECT

public:
    explicit GlobalSettings(QWidget *parent = nullptr);
    ~GlobalSettings();

private slots:
    void changeEvent(QEvent *event) override;
    void on_fpscaptureSlider_valueChanged(int);
    void on_brightnessSlider_valueChanged(int);
    void on_fpsSlider_valueChanged(int);
    void on_hide_unsupportedCheckBox_stateChanged(int);
    void on_usePreferedColorsCheckBox_stateChanged(int);
    void on_preferedColors_ColorsChanged();
    void on_randomColorsCheckBox_stateChanged(int);
    void on_audioSettings_clicked();
    void on_temperature_valueChanged(int);
    void on_tint_valueChanged(int);
    void on_startup_timeout_valueChanged(int);

private:
    Ui::GlobalSettings *ui;

    AudioSettings                   audio_settings;
};
