// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace room_ai
{
struct Vec2 { double x=0,y=0; };
struct Color { float r=0,g=0,b=0; }; // linear RGB [0,1]
inline Color Clamp(Color c){return {std::clamp(c.r,0.f,1.f),std::clamp(c.g,0.f,1.f),std::clamp(c.b,0.f,1.f)};}
inline float Decode(float v){v=std::clamp(v,0.f,1.f);return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);}
inline float Encode(float v){v=std::clamp(v,0.f,1.f);return v<=.0031308f?12.92f*v:1.055f*std::pow(v,1.f/2.4f)-.055f;}
struct Stamp
{
    std::uint64_t stream_epoch=1,state_epoch=1,layout_revision=0,sequence=0;
    double source_time=0; // monotonic seconds; demo replay uses its own explicit clock
};
struct VideoFrame
{
    unsigned width=0,height=0;
    std::shared_ptr<const std::vector<std::uint8_t>> rgb; // interleaved sRGB8, top-left
    Stamp stamp;
};
struct Binding {std::string controller_key;unsigned led_index=0;};
struct Led
{
    std::string id,group_id;
    Vec2 position;
    double radius_x=.035,radius_y=.035,gain=1;
    bool enabled=true;
    Binding binding;
};
struct Layout
{
    double screen_height=.5625;
    std::uint64_t revision=0;
    std::vector<std::pair<std::string,std::string>> groups;
    std::vector<Led> leds;
};
struct PcmBlock
{
    std::vector<float> samples; // interleaved finite floats, copied by capture
    unsigned sample_rate=48000,channels=2;
    std::uint64_t first_sample=0,stream_epoch=1;
    double source_time=0;
    bool discontinuity=false;
};
struct MusicSnapshot
{
    double time=0,level=0,onset=0,tempo_bpm=0,phase=0,confidence=0;
    std::array<float,8> bands{};
    bool valid=false,beat=false;
    std::string scene="Silence",reason="Waiting for audio",backend="Classical analysis";
};
}
