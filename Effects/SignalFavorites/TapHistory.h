// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>

namespace native_taps
{
struct Tap { double x,y,age,travel,seed; };
// Spatial impulses only. Never stores characters, keyboard scans or text.
class History
{
public:
    void Clear() { taps.clear(); }
    void Advance(double elapsed,double speed)
    {
        if(!std::isfinite(elapsed)||elapsed<0) return;
        speed=std::isfinite(speed)?std::max(0.0,speed):0;
        for(auto& tap:taps) { tap.age+=elapsed; tap.travel+=elapsed*speed; }
        while(!taps.empty()&&taps.front().age>=5) taps.pop_front();
    }
    void Add(double x,double y,double age,double speed)
    {
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(age)||age<0||age>0.25
           ||x<0||x>320||y<0||y>200) return;
        if(taps.size()==64) taps.pop_front();
        // Stable per-event seed, never a retained key identity.
        counter+=0x9e3779b9u; auto value=counter;
        value^=value>>16; value*=0x7feb352du; value^=value>>15;
        taps.push_back({x,y,age,age*std::max(0.0,speed),double(value&0xffffff)/16777216.0});
    }
    const std::deque<Tap>& Events() const { return taps; }
private:
    std::deque<Tap> taps;
    std::uint32_t counter=0;
};
}
