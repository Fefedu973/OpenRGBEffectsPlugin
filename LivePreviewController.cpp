/*---------------------------------------------------------*\
| LivePreviewController.cpp                                 |
|                                                           |
|   OpenRGB Effects Plugin Live Preview RGBController       |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "LivePreviewController.h"
#include "OpenRGBEffectsPlugin.h"
#include "OpenRGBPluginsFont.h"

LivePreviewController::LivePreviewController(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LivePreviewController)
{
    ui->setupUi(this);

    /*-----------------------------------------------------*\
    | Create Virtual RGBController                          |
    \*-----------------------------------------------------*/
    setup.object_ptr                                    = this;
    setup.name                                          = "Preview controller";
    setup.vendor                                        = "OpenRGBEffectsPlugin";
    setup.description                                   = "Preview controller provided by OpenRGBEffectsPlugin";
    setup.version                                       = VERSION_STRING;
    setup.active_mode                                   = 0;
    setup.type                                          = DEVICE_TYPE_VIRTUAL;

    setup.zones.resize(1);
    setup.modes.resize(1);

    setup.modes[0].name                                 = "Direct";
    setup.modes[0].value                                = 0;
    setup.modes[0].flags                                = MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_BRIGHTNESS;
    setup.modes[0].brightness_min                       = 0;
    setup.modes[0].brightness_max                       = 100;
    setup.modes[0].brightness                           = 100;
    setup.modes[0].color_mode                           = MODE_COLORS_PER_LED;

    setup.DeviceConfigureZone                           = nullptr;
    setup.DeviceUpdateLEDs                              = DeviceUpdateLEDs_func;
    setup.DeviceUpdateZoneLEDs                          = nullptr;
    setup.DeviceUpdateSingleLED                         = nullptr;
    setup.DeviceUpdateMode                              = nullptr;
    setup.DeviceSaveMode                                = nullptr;
    setup.DeviceUpdateZoneMode                          = nullptr;
    setup.DeviceUpdateDeviceSpecificConfiguration       = nullptr;
    setup.DeviceUpdateDeviceSpecificZoneConfiguration   = nullptr;

    controller                                          = OpenRGBEffectsPlugin::api->CreateVirtualRGBController(&setup);

    /*-----------------------------------------------------*\
    | Connect image rendered to draw                        |
    \*-----------------------------------------------------*/
    connect(this, SIGNAL(Rendered(QImage)), this, SLOT(Draw(QImage)));

    /*-----------------------------------------------------*\
    | Fill in presets list                                  |
    \*-----------------------------------------------------*/
    for(const ZonePreset& preset: presets)
    {
        ui->presets->addItem(QString::fromStdString(preset.name));
    }

    /*-----------------------------------------------------*\
    | Set up scaling                                        |
    \*-----------------------------------------------------*/
    ui->preview_widget->setScaledContents(ui->scale->isChecked());

    /*-----------------------------------------------------*\
    | Set up brightness slider                              |
    \*-----------------------------------------------------*/
    ui->brightness->setValue(100);
    ui->brightness_label->setFont(OpenRGBPluginsFont::GetFont());
    ui->brightness_label->setText(OpenRGBPluginsFont::icon(OpenRGBPluginsFont::sun));

    /*-----------------------------------------------------*\
    | Set 64x64 Matrix as default                           |
    \*-----------------------------------------------------*/
    ui->presets->setCurrentText("Matrix 64x64");

    /*-----------------------------------------------------*\
    | Enable scaled checkbox by default                     |
    \*-----------------------------------------------------*/
    ui->scale->setChecked(true);
}

LivePreviewController::~LivePreviewController()
{
    /*-----------------------------------------------------*\
    | Delete Virtual RGBController                          |
    \*-----------------------------------------------------*/
    OpenRGBEffectsPlugin::api->DeleteVirtualRGBController(controller);

    /*-----------------------------------------------------*\
    | Delete UI                                             |
    \*-----------------------------------------------------*/
    delete ui;
}

void LivePreviewController::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
}

void LivePreviewController::SetupZone(std::string name, zone_type type, unsigned int zone_width, unsigned int zone_height)
{
    unsigned int    size                                            = width * height;

    /*-----------------------------------------------------*\
    | Create zone                                           |
    \*-----------------------------------------------------*/
    setup.zones[0].name                                             = name;
    setup.zones[0].leds_count                                       = size;
    setup.zones[0].leds_min                                         = size;
    setup.zones[0].leds_max                                         = size;
    setup.zones[0].start_idx                                        = 0;
    setup.zones[0].type                                             = type;

    /*-----------------------------------------------------*\
    | Create matrix map if matrix type                      |
    \*-----------------------------------------------------*/
    if(type == ZONE_TYPE_MATRIX)
    {
        setup.zones[0].matrix_map.height                            = zone_height;
        setup.zones[0].matrix_map.width                             = zone_width;
        setup.zones[0].matrix_map.map.resize(zone_height * zone_width);

        for(unsigned int y = 0; y < zone_height; y++)
        {
            for(unsigned int x = 0; x < zone_width; x++)
            {
                setup.zones[0].matrix_map.map[(y * zone_width) + x] = (y * zone_width) + x;
            }
        }
    }

    /*-----------------------------------------------------*\
    | Update the zone in the virtual controller             |
    \*-----------------------------------------------------*/
    OpenRGBEffectsPlugin::api->UpdateVirtualRGBController(controller, &setup);
}

void LivePreviewController::DeviceUpdateLEDs()
{
    float                   brightness  = controller->GetModeBrightness(0) / 100.f;
    zone_type               type        = controller->GetZoneType(0);
    unsigned int            zone_leds   = controller->GetZoneLEDsCount(0);
    unsigned int            zone_height = controller->GetZoneMatrixMapHeight(0);
    unsigned int            zone_width  = controller->GetZoneMatrixMapWidth(0);

    if(type == ZONE_TYPE_LINEAR || type == ZONE_TYPE_SINGLE)
    {
        QImage image(zone_leds, 1, QImage::Format_ARGB32);

        for(unsigned int zone_idx = 0 ; zone_idx < zone_leds; zone_idx++)
        {
            RGBColor        rgb         = controller->GetZoneColor(0, zone_idx);
            QColor          color       = QColor(RGBGetRValue(rgb) * brightness, RGBGetGValue(rgb) * brightness, RGBGetBValue(rgb) * brightness);

            image.setPixelColor(zone_idx, 0, color);
        }

        emit Rendered(image);
    }
    else if(type == ZONE_TYPE_MATRIX)
    {
        QImage image(zone_width, zone_height, QImage::Format_ARGB32);

        for(unsigned int height = 0; height < zone_height; height++)
        {
            for(unsigned int width = 0; width < zone_width; width++)
            {
                RGBColor    rgb         = controller->GetZoneColor(0, ((height * zone_width) + width));
                QColor      color       = QColor(RGBGetRValue(rgb) * brightness, RGBGetGValue(rgb) * brightness, RGBGetBValue(rgb) * brightness);

                image.setPixelColor(width, height, color);
            }
        }

        emit Rendered(image);
    }    
}

void LivePreviewController::on_presets_currentIndexChanged(int value)
{
    const ZonePreset& preset = presets[value];

    width   = preset.width;
    height  = preset.height;

    ui->width->blockSignals(true);
    ui->height->blockSignals(true);

    ui->width->setValue(width);
    ui->height->setValue(height);

    ui->width->blockSignals(false);
    ui->height->blockSignals(false);

    Update(preset.name, preset.zt);
}

void LivePreviewController::Draw(QImage image)
{
    if(ui->preview_widget->isFullScreen())
    {
        ui->preview_widget->setPixmap(QPixmap::fromImage(image));
    }
    else
    {
        ui->preview_widget->setPixmap(QPixmap::fromImage(
                                          image.scaled(ui->preview_widget->width(),ui->preview_widget->height(),
                                                       Qt::KeepAspectRatio, Qt::FastTransformation))
                                      );
    }
}

void LivePreviewController::Update(std::string name, zone_type type)
{
    SetupZone(name, type, width, height);
}

void LivePreviewController::on_width_valueChanged(int value)
{
    width = value;
    Update("Custom", ZONE_TYPE_MATRIX);
}

void LivePreviewController::on_height_valueChanged(int value)
{
    height = value;
    Update("Custom", ZONE_TYPE_MATRIX);
}

void LivePreviewController::on_reverse_stateChanged(int value)
{
    emit ReversedChanged(value);
}

void LivePreviewController::on_scale_stateChanged(int value)
{
    ui->preview_widget->setScaledContents(value);
}

void LivePreviewController::on_brightness_valueChanged(int value)
{
    controller->SetModeBrightness(0, value);
}

void LivePreviewController::DeviceUpdateLEDs_func(void* object_ptr)
{
    ((LivePreviewController*)object_ptr)->DeviceUpdateLEDs();
}