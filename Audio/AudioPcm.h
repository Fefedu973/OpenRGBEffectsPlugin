// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace audio_pcm
{
enum class Encoding { Unsigned8, Signed16, Signed24, Signed32, Float32, Float64 };
struct Format { Encoding encoding; unsigned channels, block_align; };
inline unsigned Bytes(Encoding e)
{
    switch(e) { case Encoding::Unsigned8:return 1; case Encoding::Signed16:return 2;
    case Encoding::Signed24:return 3; case Encoding::Float64:return 8; default:return 4; }
}
inline bool Valid(const Format& f)
{
    return f.channels > 0 && f.channels <= 32 && f.block_align >= f.channels*Bytes(f.encoding) && f.block_align <= 4096;
}
inline float Sample(const uint8_t* p, Encoding e)
{
    double v=0;
    switch(e)
    {
    case Encoding::Unsigned8: v=(int(*p)-128)/128.0; break;
    case Encoding::Signed16: { int16_t n; std::memcpy(&n,p,2); v=n/32768.0; break; }
    case Encoding::Signed24: { int32_t n=p[0]|(p[1]<<8)|(p[2]<<16); if(n&0x800000)n|=int32_t(0xff000000); v=n/8388608.0; break; }
    case Encoding::Signed32: { int32_t n; std::memcpy(&n,p,4); v=n/2147483648.0; break; }
    case Encoding::Float32: { float n; std::memcpy(&n,p,4); v=n; break; }
    case Encoding::Float64: { double n; std::memcpy(&n,p,8); v=n; break; }
    }
    return std::isfinite(v) ? float(std::max(-1.0,std::min(1.0,v))) : 0.f;
}
// Decode the complete interleaved packet. The caller owns bounded storage and
// releases the WASAPI packet before performing spectral analysis on this copy.
inline bool DecodeMono(const uint8_t* data, size_t frames, const Format& f,
                       bool silent, float* output, size_t capacity)
{
    if(!Valid(f) || frames>capacity || (frames && (!output || (!silent && !data)))) return false;
    for(size_t i=0;i<frames;++i)
    {
        float sum=0;
        if(!silent) for(unsigned c=0;c<f.channels;++c)
            sum+=Sample(data+i*f.block_align+c*Bytes(f.encoding),f.encoding);
        output[i]=sum/f.channels;
    }
    return true;
}
// Latest 512 mono samples, owned by one capture. No static/shared input history.
class Window
{
public:
    bool AppendMono(const float* samples, size_t frames)
    {
        if(frames && !samples) return false;
        for(size_t i=0;i<frames;++i)
        {
            ring[next]=samples[i];
            next=(next+1)%ring.size();
        }
        return true;
    }
    bool Append(const uint8_t* data, unsigned frames, const Format& f, bool silent)
    {
        if(!Valid(f) || (!silent && !data)) return false;
        for(unsigned i=0;i<frames;++i)
        {
            float sum=0;
            if(!silent) for(unsigned c=0;c<f.channels;++c)
                sum += Sample(data+size_t(i)*f.block_align+c*Bytes(f.encoding), f.encoding);
            ring[next] = sum/f.channels;
            next=(next+1)%ring.size();
        }
        return true;
    }
    std::array<float,512> Snapshot() const
    {
        std::array<float,512> result{};
        for(size_t i=0;i<ring.size();++i) result[i]=ring[(next+i)%ring.size()];
        return result;
    }
private:
    std::array<float,512> ring{};
    size_t next=0;
};
}
