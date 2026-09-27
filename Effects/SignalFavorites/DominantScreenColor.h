// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QImage>
#include <QColor>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace native_screen_color
{
struct Result
{
    std::array<float,3> rgb{};
    float fraction=0;
    unsigned samples=0;
    bool valid=false;
};

// Music palette, not an image-average/brightness meter. Ignore dark/neutral
// pixels, group neighboring hues circularly, and normalize the selected HSV
// value to one. Pump's audio envelope remains responsible for brightness.
inline Result Dominant(const QImage& image)
{
    if(image.isNull())return {};
    struct Bucket { std::uint64_t weight=0; double hue_x=0,hue_y=0,saturation=0; unsigned samples=0; };
    std::array<Bucket,24> buckets{};
    constexpr double pi=3.14159265358979323846;
    const int width=std::min(128,image.width()),height=std::min(72,image.height());
    std::uint64_t total=0;
    unsigned samples=0;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)
    {
        const QColor c=image.pixelColor(int((x+.5)*image.width()/width),int((y+.5)*image.height()/height));
        const unsigned a=c.alpha();if(!a)continue;
        total+=a;++samples;
        const double v=c.valueF(),s=c.hsvSaturationF();
        if(v<.08 || s<.25 || v*s<.05)continue;
        const double h=c.hsvHueF();
        auto& bin=buckets[unsigned(std::floor(h*24+.5))%24];
        bin.weight+=a;bin.hue_x+=std::cos(h*2*pi)*a;bin.hue_y+=std::sin(h*2*pi)*a;
        bin.saturation+=s*a;++bin.samples;
    }
    Result result;result.samples=samples;
    if(!total)return result;
    // A family spans the center and its two neighbors, including the 359/0
    // degree boundary. Population is alpha-weighted, never value-weighted.
    Bucket best;
    for(unsigned center=0;center<24;++center)
    {
        Bucket family;
        for(unsigned index:{(center+23)%24,center,(center+1)%24})
        {
            const auto& bin=buckets[index];family.weight+=bin.weight;
            family.hue_x+=bin.hue_x;family.hue_y+=bin.hue_y;
            family.saturation+=bin.saturation;family.samples+=bin.samples;
        }
        if(family.weight>best.weight)best=family; // deterministic population ties
    }
    if(best.samples<(samples<8?1u:8u) || double(best.weight)<double(total)*.005)return result;
    double hue=std::atan2(best.hue_y,best.hue_x)/(2*pi);if(hue<0)hue+=1;
    const QColor color=QColor::fromHsvF(float(hue),float(std::clamp(best.saturation/best.weight,0.0,1.0)),1);
    result.rgb={float(color.redF()),float(color.greenF()),float(color.blueF())};
    result.fraction=float(double(best.weight)/total);result.valid=true;return result;
}

class State
{
public:
    void Reset(){result={};key=0;last_time=-1;}
    Result Update(const QImage& image,double now)
    {
        if(image.isNull() || !std::isfinite(now)){Reset();return {};}
        if(last_time>=0 && now<last_time)Reset();
        if(last_time>=0 && (key==image.cacheKey() || now-last_time<.1))return result;
        result=Dominant(image);key=image.cacheKey();last_time=now;return result;
    }
private:
    Result result;
    qint64 key=0;
    double last_time=-1;
};
}
