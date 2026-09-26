/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "CanvasImage.h"
#include <nlohmann/json.hpp>

namespace effect_canvas
{
inline nlohmann::json ValidRegions(const nlohmann::json& input)
{
    auto result = nlohmann::json::array();
    if(!input.is_array()) return result;
    for(const auto& entry : input)
    {
        if(result.size() >= 256) break;
        try
        {
            const auto& selector = entry.at("selector");
            const auto& r = entry.at("rect");
            const Region region{r.at("x"), r.at("y"), r.at("width"), r.at("height"), entry.value("rotation_deg", 0.0),
                                entry.value("flip_x", false), entry.value("flip_y", false)};
            if(!ValidRegion(region) || !selector.at("vendor").is_string()
               || !selector.at("zone_idx").is_number_integer()) continue;
            const auto zone = selector.at("zone_idx").get<std::int64_t>();
            if(zone < 0 || zone > 0xFFFFFFFFLL) continue;
            const std::string serial = selector.value("serial", std::string());
            if(serial.empty() && (selector.at("name").get<std::string>().empty()
                                 || selector.at("location").get<std::string>().empty())) continue;
            const bool segment = selector.value("is_segment", false);
            if(segment && selector.value("segment_idx", -1) < 0) continue;
            result.push_back(entry);
        }
        catch(const nlohmann::json::exception&) { /* Reject malformed mappings, keep other zones usable. */ }
    }
    return result;
}
}
