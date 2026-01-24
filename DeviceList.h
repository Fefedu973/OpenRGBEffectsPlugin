/*---------------------------------------------------------*\
| DeviceList.h                                              |
|                                                           |
|   OpenRGB Effects Plugin Device List Widget               |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QWidget>
#include "ControllerZone.h"
#include "DeviceListItem.h"

namespace Ui
{
    class DeviceList;
}

class DeviceList : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceList(QWidget *parent = nullptr);
    ~DeviceList();

    void Clear();
    void InitControllersList();

    void DisableControls();
    void EnableControls();

    void                            ApplySelection(std::vector<ControllerZone*>);
    std::vector<ControllerZone*>    GetSelection();
    std::vector<ControllerZone*>    GetControllerZones();
    bool                            GetSelectAll();

    void                            SetSelectAll(bool selectall);

signals:
    void SelectionChanged();

private slots:
    void changeEvent(QEvent *event) override;
    void on_toggle_select_all_clicked();
    void on_toggle_reverse_clicked();
    void on_toggle_brightness_clicked();

private:
    Ui::DeviceList *ui;

    std::vector<ControllerZone*>    controller_zones;
    std::vector<DeviceListItem*>    device_items;
    bool                            select_all;
};
