/*---------------------------------------------------------*\
| DeviceListItem.h                                          |
|                                                           |
|   OpenRGB Effects Plugin Device List Item Widget          |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "RGBControllerInterface.h"
#include "ControllerZone.h"
#include "ZoneListItem.h"

namespace Ui
{
    class DeviceListItem;
}

class DeviceListItem : public QWidget
{
    Q_OBJECT

public:
    DeviceListItem(std::vector<ControllerZone*>, bool has_direct);
    ~DeviceListItem();
    void SetEnabled(bool);
    void SetReverse(bool);

    void DisableControls();
    void EnableControls();
    bool HasDirect();
    void ToggleBrightnessSlider();

    std::vector<ControllerZone*> GetSelection();

    void ApplySelection(std::vector<ControllerZone*>);

signals:
    void SelectionChanged();

private slots:
    void changeEvent(QEvent *event) override;
    void on_enable_toggled(bool);
    void on_reverse_toggled(bool);
    void on_brightness_valueChanged(int);

    void OnZoneListItemEnabled(bool);
    void OnZoneListItemReversed(bool, int);
    void OnZoneListItemBrightnessChanged(int, int);

private:
    Ui::DeviceListItem*             ui;
    RGBControllerInterface*         controller;
    std::vector<ControllerZone*>    controller_zones;
    bool                            direct;
    bool                            single_zone;
    std::vector<ZoneListItem*>      zone_items;

    void RunGlobalCheckVerification();
    void SetupZonesListItems();
    void UpdateCheckState();
};
