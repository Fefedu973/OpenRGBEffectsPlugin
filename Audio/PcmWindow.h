// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

namespace room_audio
{
// audio-field-v1 input only. This is not BeatNet/OpenL3 preprocessing.
enum class PcmWindowStatus { Unavailable, Warming, Ready, Stale, UnsupportedRate, InvalidRequest, Closed };
struct PcmWindowSnapshot
{
    PcmWindowStatus status=PcmWindowStatus::Unavailable;
    std::uint64_t epoch=0, sequence=0;
    unsigned sample_rate=48000, source_rate=0, source_channels=0;
    // Seconds in the WASAPI QPC / Windows steady_clock domain. End is exclusive.
    double source_begin=0, source_end=0, captured_through=0;
    std::vector<float> mono;
    bool Ready() const { return status==PcmWindowStatus::Ready; }
};

// Single capture producer, any number of readers. Allocated lazily when a PCM
// consumer first asks the existing AudioSession for data. Never starts capture.
// Append never allocates. Its short shared lock protects only ring operations;
// readers allocate and resample outside it. A reader therefore cannot cause
// invented gaps merely by copying a five-second window.
class PcmWindowBuffer
{
public:
    static constexpr unsigned TargetRate=48000, MinimumRate=8000, MaximumRate=192000;
    static constexpr std::size_t MaximumSamples=240000; // Five seconds at 48 kHz.
    static constexpr std::size_t FilterRadius=32;
    static constexpr std::size_t Capacity=MaximumRate*5+2*FilterRadius+8;
    static constexpr double MaximumAge=0.150;

    PcmWindowBuffer():ring(Capacity) { ResetLocked(PcmWindowStatus::Unavailable); }
    PcmWindowBuffer(const PcmWindowBuffer&)=delete;
    PcmWindowBuffer& operator=(const PcmWindowBuffer&)=delete;

    void Invalidate(PcmWindowStatus reason=PcmWindowStatus::Unavailable)
    {
        std::lock_guard<std::mutex> guard(mutex);
        if(closed) return;
        ResetLocked(reason);
    }
    void Close()
    {
        std::lock_guard<std::mutex> guard(mutex);
        closed=true;
        ResetLocked(PcmWindowStatus::Closed);
    }

    bool Append(const float* mono,std::size_t count,unsigned rate,unsigned channels,
                double first_sample,bool discontinuity=false,
                bool has_device_position=false,std::uint64_t device_position=0)
    {
        std::lock_guard<std::mutex> guard(mutex);
        if(closed) return false;
        if(rate<MinimumRate || rate>MaximumRate)
        { ResetLocked(PcmWindowStatus::UnsupportedRate); return false; }
        if(!channels || channels>32 || !std::isfinite(first_sample) ||
           count>std::min<std::size_t>(384000,std::size_t(rate)*2) ||
           (count && !mono) || (has_device_position && count>UINT64_MAX-device_position))
        { ResetLocked(PcmWindowStatus::Unavailable); return false; }
        if(!count)
        {
            if(discontinuity) ResetLocked(PcmWindowStatus::Unavailable);
            return true;
        }
        const double tolerance=std::max(0.002,4.0/rate);
        const bool gap=valid && (rate!=source_rate || channels!=source_channels ||
            has_device_position!=position_known ||
            (has_device_position && device_position!=expected_position) ||
            std::abs(first_sample-captured_through)>tolerance);
        if(discontinuity || gap) ResetLocked(PcmWindowStatus::Unavailable);
        if(!valid)
        {
            source_rate=rate; source_channels=channels; anchor=first_sample;
            total=0; valid=true; status=PcmWindowStatus::Warming;
        }
        // An invalid floating sample is a data loss, not a silently invented zero.
        for(std::size_t i=0;i<count;++i)
            if(!std::isfinite(mono[i])) { ResetLocked(PcmWindowStatus::Unavailable); return false; }
        for(std::size_t i=0;i<count;++i)
        {
            ring[next]=std::max(-1.f,std::min(1.f,mono[i]));
            next=(next+1)%Capacity;
        }
        size=std::min(Capacity,size+count);
        total+=count;
        captured_through=anchor+double(total)/source_rate;
        position_known=has_device_position;
        expected_position=has_device_position?device_position+count:0;
        ++sequence;
        return true;
    }

    // Call on an inference worker, not an audio/GUI/render callback. The owned
    // source copy is bounded, and all sample-rate conversion runs after unlock.
    PcmWindowSnapshot Read(std::size_t samples,double now) const
    {
        PcmWindowSnapshot out;
        if(!samples || samples>MaximumSamples || !std::isfinite(now))
        { out.status=PcmWindowStatus::InvalidRequest; return out; }
        std::vector<float> raw;
        double raw_begin=0;
        std::size_t needed=0;
        {
            std::lock_guard<std::mutex> guard(mutex);
            out.epoch=epoch; out.sequence=sequence; out.source_rate=source_rate;
            out.source_channels=source_channels; out.captured_through=captured_through;
            out.status=status;
            if(closed || !valid) return out;
            if(now-captured_through>MaximumAge || captured_through-now>0.050)
            { out.status=PcmWindowStatus::Stale; return out; }
            needed=source_rate==TargetRate?samples:
                std::size_t(std::ceil(double(samples-1)*source_rate/TargetRate))+2*FilterRadius+2;
            if(size<needed) { out.status=PcmWindowStatus::Warming; return out; }
        }
        raw.resize(needed); // No allocation or value-initialization under the audio lock.
        {
            std::lock_guard<std::mutex> guard(mutex);
            if(closed || !valid || epoch!=out.epoch)
            { out.status=closed?PcmWindowStatus::Closed:PcmWindowStatus::Unavailable; return out; }
            // A newer same-epoch packet is welcome: always copy the latest window.
            out.sequence=sequence; out.captured_through=captured_through;
            const std::size_t begin=(next+Capacity-needed)%Capacity;
            const std::size_t first=std::min(needed,Capacity-begin);
            std::copy_n(ring.data()+begin,first,raw.data());
            std::copy_n(ring.data(),needed-first,raw.data()+first);
            raw_begin=captured_through-double(needed)/source_rate;
        }
        if(out.source_rate==TargetRate)
        {
            out.mono=std::move(raw);
            out.source_begin=raw_begin; out.source_end=out.captured_through;
        }
        else
        {
            out.mono.resize(samples);
            const double ratio=double(out.source_rate)/TargetRate;
            const double last=double(raw.size()-1-FilterRadius);
            const double first=last-double(samples-1)*ratio;
            // 64-tap Blackman-windowed sinc, 1024 fractional phases. The table
            // is built/cached only on this reader, never on the capture worker.
            // It uses only captured samples; source_end reports the look-back.
            const auto kernel=KernelFor(out.source_rate);
            for(std::size_t i=0;i<samples;++i)
            {
                const double position=first+double(i)*ratio;
                int center=int(std::floor(position));
                unsigned phase=unsigned(std::floor((position-center)*Kernel::Phases+0.5));
                if(phase==Kernel::Phases) { phase=0; ++center; }
                double value=0;
                const auto& weights=kernel->weights[phase];
                for(unsigned tap=0;tap<2*FilterRadius;++tap)
                {
                    const int index=center+int(tap)-int(FilterRadius)+1;
                    value+=raw[std::size_t(index)]*weights[tap];
                }
                out.mono[i]=float(std::max(-1.0,std::min(1.0,value)));
            }
            out.source_begin=raw_begin+first/out.source_rate;
            out.source_end=raw_begin+last/out.source_rate+1.0/TargetRate;
        }
        // Reconnect/invalidation while converting must not publish an old window.
        {
            std::lock_guard<std::mutex> guard(mutex);
            if(closed || !valid || epoch!=out.epoch)
            { out.mono.clear(); out.status=closed?PcmWindowStatus::Closed:PcmWindowStatus::Unavailable; return out; }
        }
        out.status=PcmWindowStatus::Ready;
        return out;
    }

private:
    struct Kernel
    {
        static constexpr unsigned Phases=1024;
        unsigned rate=0;
        std::vector<std::array<float,2*FilterRadius>> weights;
        explicit Kernel(unsigned sample_rate):rate(sample_rate),weights(Phases)
        {
            const double cutoff=0.94*std::min(1.0,double(TargetRate)/rate);
            constexpr double pi=3.14159265358979323846;
            for(unsigned phase=0;phase<Phases;++phase)
            {
                double sum=0;
                for(unsigned tap=0;tap<2*FilterRadius;++tap)
                {
                    const double distance=double(tap)-FilterRadius+1-double(phase)/Phases;
                    const double x=distance/FilterRadius;
                    const double window=std::abs(x)>=1?0:0.42+0.5*std::cos(pi*x)+0.08*std::cos(2*pi*x);
                    const double a=pi*cutoff*distance;
                    const double w=cutoff*(std::abs(a)<1e-12?1.0:std::sin(a)/a)*window;
                    weights[phase][tap]=float(w); sum+=w;
                }
                for(float& weight:weights[phase]) weight=float(weight/sum);
            }
        }
    };
    std::shared_ptr<const Kernel> KernelFor(unsigned rate) const
    {
        std::lock_guard<std::mutex> guard(kernel_mutex);
        if(!cached_kernel || cached_kernel->rate!=rate) cached_kernel=std::make_shared<Kernel>(rate);
        return cached_kernel;
    }
    mutable std::mutex kernel_mutex;
    mutable std::shared_ptr<const Kernel> cached_kernel;
    static inline std::atomic<std::uint64_t> next_epoch{1};
    mutable std::mutex mutex;
    std::vector<float> ring;
    std::size_t size=0,next=0;
    std::uint64_t epoch=0,sequence=0,total=0,expected_position=0;
    unsigned source_rate=0,source_channels=0;
    double anchor=0,captured_through=0;
    bool valid=false,closed=false,position_known=false;
    PcmWindowStatus status=PcmWindowStatus::Unavailable;
    void ResetLocked(PcmWindowStatus reason)
    {
        epoch=next_epoch.fetch_add(1,std::memory_order_relaxed);
        size=next=0; total=expected_position=0; source_rate=source_channels=0;
        anchor=captured_through=0; valid=position_known=false; status=reason;
    }
};
}
