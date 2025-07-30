/*---------------------------------------------------------*\
| ZoneListItem.cpp                                          |
|                                                           |
|   OpenRGB Effects Plugin Zone List Item Widget            |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "ZoneListItem.h"
#include "ui_ZoneListItem.h"
#include "OpenRGBPluginsFont.h"

ZoneListItem::ZoneListItem(ControllerZone* controller_zone) :
    QWidget(nullptr),
    controller_zone(controller_zone),
    ui(new Ui::ZoneListItem)
{
    std::string name = controller_zone->display_name();

    ui->setupUi(this);
    ui->brightness->setVisible(false);
    ui->zone_name->setText(QString::fromStdString("• " + name));

    ui->enable->setFont(OpenRGBPluginsFont::GetFont());
    ui->reverse->setFont(OpenRGBPluginsFont::GetFont());

    ui->reverse->setText(OpenRGBPluginsFont::icon(OpenRGBPluginsFont::arrows_exchange));

    UpdateCheckState();
}

ZoneListItem::~ZoneListItem()
{
    delete ui;
}

void ZoneListItem::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
}

ControllerZone* ZoneListItem::GetControllerZone()
{
    return controller_zone;
}

void ZoneListItem::DisableControls()
{
    ui->enable->setDisabled(true);
    ui->reverse->setDisabled(true);
}

void ZoneListItem::EnableControls()
{
    ui->enable->setDisabled(false);
    ui->reverse->setDisabled(false);
}

void ZoneListItem::ToggleBrightnessSlider()
{
    ui->brightness->setVisible(!ui->brightness->isVisible());
}

void ZoneListItem::SetEnableChecked(bool state)
{
    ui->enable->blockSignals(true);
    ui->enable->setChecked(state);
    ui->enable->blockSignals(false);
    UpdateCheckState();
}

void ZoneListItem::SetReverseChecked(bool state)
{
    ui->reverse->blockSignals(true);
    ui->reverse->setChecked(state);
    ui->reverse->blockSignals(false);
}

void ZoneListItem::on_enable_toggled(bool state)
{
    UpdateCheckState();
    emit Enabled(state);
}

void ZoneListItem::on_reverse_toggled(bool state)
{
    emit Reversed(state);
}

void ZoneListItem::on_brightness_valueChanged(int value)
{
    emit BrightnessChanged(value);
}

bool ZoneListItem::HasSegments()
{
    return controller_zone->has_segments();
}

bool ZoneListItem::IsEnabled()
{
    return ui->enable->isChecked();
}

bool ZoneListItem::IsReversed()
{
    return ui->reverse->isChecked();
}

bool ZoneListItem::IsSegment()
{
    return controller_zone->is_segment;
}

void ZoneListItem::SetBrightness(int value)
{
    ui->brightness->setValue(value);
}

void ZoneListItem::UpdateCheckState()
{
    ui->enable->setText(
                ui->enable->isChecked()?
                    OpenRGBPluginsFont::icon(OpenRGBPluginsFont::check_o):
                    OpenRGBPluginsFont::icon(OpenRGBPluginsFont::check)
                    );
}
