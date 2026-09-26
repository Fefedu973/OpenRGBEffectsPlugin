/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "CanvasRegions.h"
#include "ControllerZone.h"
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#if __has_include(<FrameRouting/RGBControllerImageInterface.h>)
#include <FrameRouting/RGBControllerImageInterface.h>
#define EFFECTS_HAS_NATIVE_IMAGE_ROUTING 1
#endif

namespace effect_canvas
{
inline Region RegionFor(ControllerZone* zone, const nlohmann::json& regions)
{
    for(const auto& entry : regions)
    {
        const auto& selector = entry["selector"];
        if(selector["zone_idx"] != zone->zone_idx || selector.value("is_segment", false) != zone->is_segment) continue;
        if(zone->is_segment && selector.value("segment_idx", -1) != zone->segment_idx) continue;
        if(selector["vendor"] != zone->controller->GetVendor()) continue;
        const std::string serial = selector.value("serial", std::string());
        if(!serial.empty()) { if(serial != zone->controller->GetSerial()) continue; }
        else if(selector["name"] != zone->controller->GetName() || selector["location"] != zone->controller->GetLocation()) continue;
        const auto& r = entry["rect"];
        return {r["x"], r["y"], r["width"], r["height"], entry.value("rotation_deg", 0.0),
                entry.value("flip_x", false), entry.value("flip_y", false)};
    }
    return {};
}

// One adjusted allocation per source/settings revision, shared by every native
// image sink and the optional external FrameSurface publisher. No per-device copy.
class FrameCache
{
public:
    void Update(const QImage& image, std::uint64_t source_sequence, float brightness, int temperature, int tint)
    {
        if(image.isNull()) return;
        if(pixels && source_sequence == input_sequence && brightness == last_brightness
           && temperature == last_temperature && tint == last_tint) return;
        const QImage source = image.convertToFormat(QImage::Format_RGB32);
        auto storage = std::make_shared<std::vector<std::uint8_t>>(std::size_t(source.width()) * source.height() * 4);
        RGBColor tables[256];
        for(int value = 0; value < 256; ++value)
            tables[value] = ColorUtils::apply_adjustments(ToRGBColor(value,value,value), std::clamp(brightness / 100.f, 0.f, 1.f), temperature, tint);
        for(int y = 0; y < source.height(); ++y)
        {
            const auto* row = reinterpret_cast<const QRgb*>(source.constScanLine(y));
            auto* out = storage->data() + std::size_t(y) * source.width() * 4;
            for(int x = 0; x < source.width(); ++x)
            {
                out[4*x] = RGBGetBValue(tables[qBlue(row[x])]);
                out[4*x+1] = RGBGetGValue(tables[qGreen(row[x])]);
                out[4*x+2] = RGBGetRValue(tables[qRed(row[x])]);
                out[4*x+3] = 255;
            }
        }
        width = source.width(); height = source.height(); stride = width * 4;
        pixels = std::move(storage); ++sequence;
        input_sequence = source_sequence; last_brightness = brightness;
        last_temperature = temperature; last_tint = tint;
#ifdef EFFECTS_HAS_NATIVE_IMAGE_ROUTING
        auto next = std::make_shared<room_image::Frame>();
        next->width = width; next->height = height; next->stride = stride;
        next->sequence = sequence; next->pixels = pixels; frame = std::move(next);
#endif
    }
    std::uint32_t width = 0, height = 0, stride = 0;
    std::uint64_t sequence = 0;
    std::shared_ptr<const std::vector<std::uint8_t>> pixels;
#ifdef EFFECTS_HAS_NATIVE_IMAGE_ROUTING
    std::shared_ptr<const room_image::Frame> frame;
#endif
private:
    std::uint64_t input_sequence = 0;
    float last_brightness = -1;
    int last_temperature = 0, last_tint = 0;
};

class LedPlans
{
public:
    const std::vector<Sample>& Get(ControllerZone* zone, std::uint64_t revision, const Region& region)
    {
        const auto type = zone->type();
        const bool matrix = type == ZONE_TYPE_MATRIX || type == ZONE_TYPE_MATRIX_LOOP_X || type == ZONE_TYPE_MATRIX_LOOP_Y;
        if(!matrix && type != ZONE_TYPE_SINGLE && type != ZONE_TYPE_LINEAR && type != ZONE_TYPE_LINEAR_LOOP) return empty;
        const unsigned count = zone->leds_count(), width = matrix ? zone->matrix_map_width() : count;
        const unsigned height = matrix ? zone->matrix_map_height() : 1;
        const unsigned* map = matrix ? zone->map() : nullptr;
        if(matrix && !map) return empty;
        auto found = plans.find(zone);
        if(found == plans.end() || found->second.revision != revision || found->second.width != width
           || found->second.height != height || found->second.count != count || found->second.map != map
           || found->second.reverse != zone->reverse)
        {
            if(plans.size() >= 1024) plans.clear();
            Plan plan{revision,width,height,count,map,zone->reverse,BuildPlan(width,height,count,map,zone->reverse,region)};
            found = plans.insert_or_assign(zone,std::move(plan)).first;
        }
        return found->second.samples;
    }
private:
    struct Plan
    {
        std::uint64_t revision;
        unsigned width,height,count;
        const unsigned* map;
        bool reverse;
        std::vector<Sample> samples;
    };
    std::map<ControllerZone*,Plan> plans;
    std::vector<Sample> empty;
};

class Router
{
public:
    FrameCache frame;
    void SetRunning(bool value)
    {
#ifdef EFFECTS_HAS_NATIVE_IMAGE_ROUTING
        std::lock_guard<std::mutex> guard(route_mutex);
        running = value;
        submitted.clear();
#else
        (void)value;
#endif
    }
    bool Route(ControllerZone* zone, const QImage& source, std::uint64_t sequence,
               float brightness, int temperature, int tint, const Region& region)
    {
#ifdef EFFECTS_HAS_NATIVE_IMAGE_ROUTING
        if(zone->is_segment) return false;
        return Submit(dynamic_cast<room_image::RGBControllerImageInterface*>(zone->controller), zone->zone_idx,
                      source, sequence, brightness, temperature, tint, region, zone->reverse,
                      std::clamp(zone->self_brightness / 100.0, 0.0, 1.0));
#else
        (void)zone; (void)source; (void)sequence; (void)brightness; (void)temperature; (void)tint; (void)region;
        return false;
#endif
    }
#ifdef EFFECTS_HAS_NATIVE_IMAGE_ROUTING
    bool Submit(room_image::RGBControllerImageInterface* sink, unsigned zone, const QImage& source,
                std::uint64_t sequence, float brightness, int temperature, int tint,
                const Region& region, bool reverse, double gain = 1.0)
    {
        std::lock_guard<std::mutex> guard(route_mutex);
        if(!running) return true;
        if(!sink) return false;
        room_image::Output output;
        if(!sink->GetImageOutput(zone, output)) return false;
        if(!ValidRegion(region) || !output.width || !output.height) return true;
        const auto key = std::make_pair(sink, zone);
        const auto now = std::chrono::steady_clock::now();
        const auto found = submitted.find(key);
        const unsigned fps = output.max_fps ? std::clamp(output.max_fps, 1u, 240u) : 60u;
        if(found != submitted.end() && now - found->second < std::chrono::microseconds(1000000 / fps)) return true;
        frame.Update(source, sequence, brightness, temperature, tint);
        auto mapping = room_image::Mapping::Rectangle(region.x,region.y,region.width,region.height,
                                                            region.rotation, region.flip_x != reverse,region.flip_y);
        mapping.brightness = std::clamp(gain, 0.0, 1.0);
        const auto result = sink->SubmitImage(zone,frame.frame,mapping,1000);
        if(result == room_image::SubmitResult::Unsupported) return false;
        if(submitted.size() > 1024) submitted.clear();
        submitted[key] = now;
        return true; // Accepted, Busy and Invalid all preserve the native image route.
    }
private:
    std::mutex route_mutex;
    bool running = true;
    std::map<std::pair<room_image::RGBControllerImageInterface*,unsigned>,std::chrono::steady_clock::time_point> submitted;
#endif
};
}
