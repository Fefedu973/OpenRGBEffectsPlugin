/*---------------------------------------------------------*\
| ControllerZone.h                                          |
|                                                           |
|   OpenRGB Effects Plugin Controller Zone                  |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <nlohmann/json.hpp>
#include "ColorUtils.h"
#include "RGBControllerInterface.h"

class ControllerZone
{
public:
    ControllerZone
        (
        RGBControllerInterface* controller,
        unsigned int            zone_idx,
        bool                    reverse,
        int                     self_brightness,
        bool                    has_direct,
        bool                    is_segment,
        int                     segment_idx = -1
        );

    std::vector<RGBColor>       colors();
    RGBColor*                   colors_ptr();
    std::string                 display_name();
    RGBColor                    GetLED(int idx);
    bool                        has_segments();
    unsigned int                leds_count();
    const unsigned int*         map();
    unsigned int                matrix_map_height();
    unsigned int                matrix_map_width();
    unsigned int                matrix_size();
    void                        SetAllZoneLEDs(RGBColor color, int brightness, int temperature, int tint);
    void                        SetLED(int idx, RGBColor color, int brightness, int temperature, int tint);
    unsigned int                size();
    unsigned int                start_idx();
    nlohmann::json              to_json();
    zone_type                   type();
    unsigned int                zone_start_idx();
    unsigned int                zone_stop_idx();

    bool operator==(ControllerZone const & rhs) const
    {        
        return(this->controller == rhs.controller && this->zone_idx == rhs.zone_idx && this->segment_idx == rhs.segment_idx);
    }

    bool operator<(ControllerZone const & rhs) const
    {
        if(is_segment)
        {
            return(this->controller != rhs.controller || this->zone_idx != rhs.zone_idx || this->segment_idx != rhs.segment_idx);
        }
        else
        {
            return(this->controller != rhs.controller || this->zone_idx != rhs.zone_idx);
        }
    }

    RGBControllerInterface*     controller;
    unsigned int                zone_idx;
    bool                        reverse;
    unsigned int                self_brightness;
    bool                        has_direct;
    bool                        is_segment;
    int                         segment_idx;
};
