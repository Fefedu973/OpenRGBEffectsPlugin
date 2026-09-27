// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QImage>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace native_screen_color
{
struct Result
{
    std::array<float,3> rgb{};
    float fraction=0;
    unsigned samples=0;
    bool valid=false;
};

// Spatially uniform nearest-pixel sampling: no interpolation invents gray
// between different colors. The most populous RGB4 bucket wins; only pixels
// in that bucket are averaged. Black/neutral pixels are not silently excluded.
inline Result Dominant(const QImage& image)
{
    if(image.isNull())return {};
    struct Bucket { std::uint64_t weight=0,r=0,g=0,b=0; unsigned samples=0; };
    std::vector<Bucket> buckets(4096);
    const int width=std::min(128,image.width()),height=std::min(72,image.height());
    std::uint64_t total=0;
    unsigned samples=0;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)
    {
        const QColor c=image.pixelColor(int((x+.5)*image.width()/width),int((y+.5)*image.height()/height));
        const unsigned a=c.alpha();if(!a)continue;
        auto& bin=buckets[(c.red()>>4)*256+(c.green()>>4)*16+(c.blue()>>4)];
        bin.weight+=a;bin.r+=c.red()*a;bin.g+=c.green()*a;bin.b+=c.blue()*a;++bin.samples;
        total+=a;++samples;
    }
    if(!total)return {};
    const auto best=std::max_element(buckets.begin(),buckets.end(),[](const Bucket&a,const Bucket&b){return a.weight<b.weight;});
    const double denominator=double(best->weight)*255;
    return {{{float(best->r/denominator),float(best->g/denominator),float(best->b/denominator)}},
            float(double(best->weight)/total),samples,true};
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
