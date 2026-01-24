/*---------------------------------------------------------*\
| ZoneListItem.h                                            |
|                                                           |
|   OpenRGB Effects Plugin Zone List Item Widget            |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ControllerZone.h"

namespace Ui
{
    class ZoneListItem;
}

class ZoneListItem : public QWidget
{
    Q_OBJECT

public:
    ZoneListItem(ControllerZone*);
    ~ZoneListItem();

    void SetEnableChecked(bool);
    void SetReverseChecked(bool);

    void DisableControls();
    void EnableControls();
    void ToggleBrightnessSlider();

    bool IsEnabled();
    bool IsReversed();

    void SetBrightness(int);

    ControllerZone* GetControllerZone();

private slots:
    void changeEvent(QEvent *event) override;
    void on_enable_toggled(bool);
    void on_reverse_toggled(bool);
    void on_brightness_valueChanged(int);

signals:
    void Enabled(bool);
    void Reversed(bool);
    void BrightnessChanged(int);

private:
    ControllerZone* controller_zone;
    Ui::ZoneListItem *ui;
    void UpdateCheckState();
};
