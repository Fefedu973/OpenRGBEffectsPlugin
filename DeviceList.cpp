/*---------------------------------------------------------*\
| DeviceList.cpp                                            |
|                                                           |
|   OpenRGB Effects Plugin Device List Widget               |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <QVBoxLayout>
#include "DeviceList.h"
#include "ui_DeviceList.h"
#include "OpenRGBPluginsFont.h"
#include "OpenRGBEffectsPlugin.h"
#include "OpenRGBEffectSettings.h"

DeviceList::DeviceList(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DeviceList)
{
    ui->setupUi(this);
    ui->devices->setLayout(new QVBoxLayout(ui->devices));

    ui->toggle_brightness->setFont(OpenRGBPluginsFont::GetFont());
    ui->toggle_brightness->setText(OpenRGBPluginsFont::icon(OpenRGBPluginsFont::sun));

    select_all = false;
}

DeviceList::~DeviceList()
{
    delete ui;
}

void DeviceList::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
}

void DeviceList::Clear()
{
    device_items.clear();

    QLayoutItem *child;

    while((child = ui->devices->layout()->takeAt(0)) != 0)
    {
        delete child->widget();
    }

    /*-----------------------------------------------------*\
    | No SelectionChanged on teardown: an empty selection   |
    | here would wipe the visible effect's assignment       |
    | before it is remapped                                 |
    \*-----------------------------------------------------*/
}

void DeviceList::UpdateDeviceList()
{
    /*-----------------------------------------------------*\
    | Clear the device list                                 |
    \*-----------------------------------------------------*/
    Clear();

    /*-----------------------------------------------------*\
    | Group all ControllerZones for the same controller and |
    | create DeviceListItems for each group                 |
    \*-----------------------------------------------------*/
    RGBControllerInterface*         current_controller;
    std::vector<ControllerZone*>    current_controller_zones;

    current_controller = NULL;

    for(ControllerZone* controller_zone : OpenRGBEffectsPlugin::controller_zones)
    {
        if(controller_zone->controller != current_controller)
        {
            if(current_controller_zones.size() > 0)
            {
                DeviceListItem* item = new DeviceListItem(current_controller_zones, current_controller_zones[0]->has_direct);
                ui->devices->layout()->addWidget(item);
                device_items.push_back(item);

                connect(item, &DeviceListItem::SelectionChanged, [=](){
                    emit SelectionChanged();
                });
            }

            current_controller_zones.clear();
        }

        current_controller_zones.push_back(controller_zone);
        current_controller = controller_zone->controller;
    }
    
    if(current_controller_zones.size() > 0)
    {
        DeviceListItem* item = new DeviceListItem(current_controller_zones, current_controller_zones[0]->has_direct);
        ui->devices->layout()->addWidget(item);
        device_items.push_back(item);

        connect(item, &DeviceListItem::SelectionChanged, [=](){
            emit SelectionChanged();
        });
    }

    if(select_all)
    {
        for(DeviceListItem* item: device_items)
        {
            if(item->HasDirect())
            {
                item->SetEnabled(true);
            }
        }

        emit SelectionChanged();
    }

    ((QVBoxLayout*) ui->devices->layout())->addStretch(10000);
}

void DeviceList::on_toggle_select_all_clicked()
{
    select_all = ui->toggle_select_all->isChecked();

    for(DeviceListItem* item: device_items)
    {
        if(select_all && item->HasDirect())
        {
            item->SetEnabled(true);
        }
        else
        {
            item->SetEnabled(false);
        }
    }

    emit SelectionChanged();
}

void DeviceList::on_toggle_reverse_clicked()
{
    for(DeviceListItem* item: device_items)
    {
        item->SetReverse(ui->toggle_reverse->isChecked());
    }

    emit SelectionChanged();
}

void DeviceList::on_toggle_brightness_clicked()
{
    for(DeviceListItem* item: device_items)
    {
        item->ToggleBrightnessSlider();
    }
}

void DeviceList::DisableControls()
{
    setEnabled(false);

    for(DeviceListItem* item: device_items)
    {
        item->DisableControls();
    }
}

void DeviceList::EnableControls()
{
   setEnabled(true);

    for(DeviceListItem* item: device_items)
    {
        item->EnableControls();
    }
}

std::vector<ControllerZone*> DeviceList::GetSelection()
{
    std::vector<ControllerZone*> selection;

    for(DeviceListItem* item: device_items)
    {
        for(ControllerZone* controller_zone: item->GetSelection())
        {
            selection.push_back(controller_zone);
        }
    }

    return selection;
}

bool DeviceList::GetSelectAll()
{
    return select_all;
}

void DeviceList::SetSelectAll(bool selectall)
{
    select_all = selectall;

    ui->toggle_select_all->setChecked(select_all);

    for(DeviceListItem* item: device_items)
    {
        if(select_all && item->HasDirect())
        {
            item->SetEnabled(true);
        }
        else
        {
            item->SetEnabled(false);
        }
    }

    emit SelectionChanged();
}

void DeviceList::ApplySelection(std::vector<ControllerZone*> selection)
{
    for(DeviceListItem* item: device_items)
    {
        item->ApplySelection(selection);
    }
}
