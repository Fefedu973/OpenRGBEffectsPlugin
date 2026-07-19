/*---------------------------------------------------------*\
| ControllerZone.cpp                                        |
|                                                           |
|   OpenRGB Effects Plugin Controller Zone                  |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "ControllerZone.h"

ControllerZone::ControllerZone
    (
    RGBControllerInterface* controller,
    unsigned int            zone_idx,
    bool                    reverse,
    int                     self_brightness,
    bool                    has_direct,
    bool                    is_segment,
    int                     segment_idx
    )
{
    this->controller        = controller;
    this->zone_idx          = zone_idx;
    this->reverse           = reverse;
    this->self_brightness   = self_brightness;
    this->has_direct        = has_direct;
    this->is_segment        = is_segment;
    this->segment_idx       = segment_idx;
}

std::vector<RGBColor> ControllerZone::colors()
{
    std::vector<RGBColor> color_data;

    RGBColor* color_ptr = colors_ptr();

    for(unsigned int i = zone_start_idx(); i < zone_stop_idx(); i ++)
    {
        color_data.push_back(color_ptr[i]);
    }

    return(color_data);
}

RGBColor* ControllerZone::colors_ptr()
{
    return(controller->GetZoneColorsPointer(zone_idx));
}

std::string ControllerZone::display_name()
{
    return(controller->GetName() + ": " +  controller->GetZoneName(zone_idx) + (is_segment ? (" - " + controller->GetZoneSegmentName(zone_idx, segment_idx)) : ""));
}

RGBColor ControllerZone::GetLED(int idx)
{
    return(controller->GetColor(start_idx() + idx));
}

bool ControllerZone::has_segments()
{
    return(is_segment ? false : (controller->GetZoneSegmentCount(zone_idx) > 0));
}

unsigned int ControllerZone::leds_count()
{
    return(is_segment ? controller->GetZoneSegmentLEDsCount(zone_idx, segment_idx) : controller->GetZoneLEDsCount(zone_idx));
}

const unsigned int* ControllerZone::map()
{
    return(is_segment ? controller->GetZoneSegmentMatrixMapData(zone_idx, segment_idx) : controller->GetZoneMatrixMapData(zone_idx));
}

unsigned int ControllerZone::matrix_map_height()
{
    return(is_segment ? controller->GetZoneSegmentMatrixMapHeight(zone_idx, segment_idx) : controller->GetZoneMatrixMapHeight(zone_idx));
}

unsigned int ControllerZone::matrix_map_width()
{
    return(is_segment ? controller->GetZoneSegmentMatrixMapWidth(zone_idx, segment_idx) : controller->GetZoneMatrixMapWidth(zone_idx));
}

unsigned int ControllerZone::matrix_size()
{
    return(matrix_map_width() * matrix_map_height());
}

void ControllerZone::SetAllZoneLEDs(RGBColor color, int brightness, int temperature, int tint)
{
    if(is_segment)
    {
        for(unsigned int i = 0; i < leds_count(); i ++)
        {
            SetLED(i, color, brightness, temperature, tint);
        }
    }
    else
    {
        controller->SetAllZoneColors(zone_idx, ColorUtils::apply_adjustments(color, (self_brightness / 100.f) * (brightness / 100.f), temperature, tint));
    }
}

void ControllerZone::SetLED(int idx, RGBColor color, int brightness, int temperature, int tint)
{
    if(idx != 0xFFFFFFFF)
    {
        controller->SetColor(start_idx() + idx, ColorUtils::apply_adjustments(color, (self_brightness / 100.f) * (brightness / 100.f), temperature, tint));
    }
}

unsigned int ControllerZone::size()
{
    return(type() == ZONE_TYPE_MATRIX ? matrix_size() : leds_count());
}

unsigned int ControllerZone::start_idx()
{
    return(is_segment ? (controller->GetZoneStartIndex(zone_idx) + controller->GetZoneSegmentStartIndex(zone_idx, segment_idx)) : controller->GetZoneStartIndex(zone_idx));
}

nlohmann::json ControllerZone::to_json()
{
    nlohmann::json controller_zone_json;

    controller_zone_json["zone_idx"]        = zone_idx;
    controller_zone_json["reverse"]         = reverse;
    controller_zone_json["self_brightness"] = self_brightness;
    controller_zone_json["name"]            = controller->GetName();
    controller_zone_json["location"]        = controller->GetLocation();
    controller_zone_json["serial"]          = controller->GetSerial();
    controller_zone_json["description"]     = controller->GetDescription();
    controller_zone_json["version"]         = controller->GetVersion();
    controller_zone_json["vendor"]          = controller->GetVendor();
    controller_zone_json["is_segment"]      = is_segment;
    controller_zone_json["segment_idx"]     = segment_idx;

    return(controller_zone_json);
}

zone_type ControllerZone::type()
{
    return(is_segment ? controller->GetZoneSegmentType(zone_idx, segment_idx) : controller->GetZoneType(zone_idx));
}

unsigned int ControllerZone::zone_start_idx()
{
    return(is_segment ? controller->GetZoneSegmentStartIndex(zone_idx, segment_idx) : 0);
}

unsigned int ControllerZone::zone_stop_idx()
{
    return(zone_start_idx() + leds_count());
}
