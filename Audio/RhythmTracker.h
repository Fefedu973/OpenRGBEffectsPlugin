// SPDX-License-Identifier: GPL-2.0-or-later
// Continuous, single-owner PCM analysis. No renderer, device or wall-clock I/O.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>

namespace room_audio
{
struct RhythmSnapshot
{
    std::uint64_t generation=0, sequence=0, onset_sequence=0;
    double audio_time=0.0, last_onset_time=-1.0;
    float onset_strength=0.0f;
    std::array<float,3> band_flux{}; // Positive spectral changes: 40-250, 250-2000, 2000-12000 Hz.
    float bpm=0.0f, phase=0.0f, confidence=0.0f;
    bool locked=false, silent=true;
};

class RhythmTracker
{
public:
    // Call only from the capture owner. Publish copies of Snapshot() under the
    // capture session mutex; do not call this object from the render thread.
    RhythmTracker() { Reset(); }
    void Reset(double audio_time=0.0)
    {
        const auto generation=state.generation+1;
        const auto sequence=state.sequence, onsets=state.onset_sequence;
        state={}; state.generation=generation; state.sequence=sequence;
        state.onset_sequence=onsets;
        state.audio_time=std::isfinite(audio_time)?audio_time:0.0;
        ring.fill(0); previous_magnitudes.fill(0); history.fill(0);
        flux_mean.fill(0); events.fill({});
        next=filled=history_next=history_count=event_next=event_count=0;
        hop_samples=0;
        sample_rate=fft_size=0; sample_count=0;
        previous_novelty=older_novelty=previous_raw=0; previous_peak_time=0;
        last_event=-1000; audible_time=-1000; onset_peak=0; candidate_period=0;
        candidate_repeats=0; estimate_countdown=0; stream=false; phase_origin=0;
    }

    // Timestamp is the first sample's monotonic AUDIO time, in seconds. It must
    // advance by count/sample_rate even if the renderer stalls. Packet boundaries
    // are immaterial. Pass discontinuity on lost frames or endpoint changes.
    // Rates 8-192 kHz and packets <=2 seconds are bounded; no internal allocation.
    bool Push(const float* mono, std::size_t count, unsigned rate,
              double first_sample_seconds, bool discontinuity=false)
    {
        if(rate<8000 || rate>192000 || count>std::size_t(rate)*2 ||
           (count && !mono) || !std::isfinite(first_sample_seconds))
        { Reset(); return false; }
        if(!count) { if(discontinuity) Reset(first_sample_seconds); return true; }
        const double tolerance=std::max(2.0/rate,0.002);
        if(discontinuity || (stream && (rate!=sample_rate ||
           std::abs(first_sample_seconds-(origin+double(sample_count)/sample_rate))>tolerance)))
            Reset(first_sample_seconds);
        if(!stream)
        {
            stream=true; sample_rate=rate; origin=first_sample_seconds;
            fft_size=256; while(fft_size<rate*0.040 && fft_size<MaxFFT) fft_size*=2;
            for(std::size_t i=0;i<fft_size;++i)
                window[i]=float(0.5-0.5*std::cos(Tau*double(i)/double(fft_size-1)));
        }
        for(std::size_t i=0;i<count;++i)
        {
            ring[next]=std::isfinite(mono[i])?std::clamp(mono[i],-1.0f,1.0f):0.0f;
            next=(next+1)%fft_size; filled=std::min(filled+1,fft_size);
            ++sample_count;
            hop_samples+=100;
            if(hop_samples>=sample_rate)
            {
                hop_samples-=sample_rate;
                if(filled==fft_size) Analyze(origin+double(sample_count)/sample_rate);
            }
        }
        return true;
    }

    RhythmSnapshot Snapshot() const { return state; }

private:
    static constexpr std::size_t MaxFFT=8192, History=800, MaxEvents=128;
    static constexpr double Tau=6.2831853071795864769;
    RhythmSnapshot state{};
    std::array<float,MaxFFT> ring{}, window{};
    std::array<float,MaxFFT/2> previous_magnitudes{};
    std::array<std::complex<float>,MaxFFT> spectrum{};
    std::array<float,History> history{};
    std::array<float,3> flux_mean{};
    struct Event { double time=0; float weight=0; };
    std::array<Event,MaxEvents> events{};
    std::size_t next=0,filled=0,fft_size=0,history_next=0,history_count=0,event_next=0,event_count=0;
    unsigned sample_rate=0,hop_samples=0,estimate_countdown=0,candidate_repeats=0;
    std::uint64_t sample_count=0;
    bool stream=false;
    double origin=0,last_event=-1000,audible_time=-1000,previous_peak_time=0,phase_origin=0;
    float previous_novelty=0,older_novelty=0,previous_raw=0,onset_peak=0,candidate_period=0;

    void FFT()
    {
        for(std::size_t i=1,j=0;i<fft_size;++i)
        {
            std::size_t bit=fft_size>>1;
            for(;j&bit;bit>>=1) j^=bit;
            j^=bit;
            if(i<j) std::swap(spectrum[i],spectrum[j]);
        }
        for(std::size_t length=2;length<=fft_size;length*=2)
        {
            const auto step=std::polar(1.0f,float(-Tau/length));
            for(std::size_t base=0;base<fft_size;base+=length)
            {
                std::complex<float> phase(1.0f,0.0f);
                for(std::size_t j=0;j<length/2;++j)
                {
                    const auto a=spectrum[base+j], b=spectrum[base+j+length/2]*phase;
                    spectrum[base+j]=a+b; spectrum[base+j+length/2]=a-b;
                    phase*=step;
                }
            }
        }
    }

    void ClearTempo()
    {
        history.fill(0); history_next=history_count=0;
        events.fill({}); event_next=event_count=0; candidate_repeats=0; candidate_period=0;
        state.locked=false; state.confidence=state.phase=state.bpm=0;
    }

    void Analyze(double end_time)
    {
        ++state.sequence; state.audio_time=end_time;
        double power=0;
        for(std::size_t i=0;i<fft_size;++i)
        {
            const float x=ring[(next+i)%fft_size]; power+=double(x)*x;
            spectrum[i]=std::complex<float>(x*window[i],0);
        }
        const float rms=float(std::sqrt(power/fft_size));
        if(rms>0.0003f) audible_time=end_time;
        // Short rests are part of a rhythm; silence is declared only after a
        // complete slow-beat interval. Missing capture packets are a separate
        // transport condition: consumers must not extrapolate a stale snapshot.
        state.silent=end_time-audible_time>1.10;
        state.onset_strength*=0.9200444f; // 120 ms release, independent of renderer FPS.
        if(end_time-audible_time>1.10)
        {
            if(history_count || state.locked) ClearTempo();
            previous_magnitudes.fill(0); flux_mean.fill(0);
            previous_novelty=older_novelty=previous_raw=0; state.onset_strength=0; state.band_flux.fill(0);
            return;
        }
        FFT();
        std::array<float,3> flux{};
        std::array<unsigned,3> bins{};
        for(std::size_t bin=1;bin<fft_size/2;++bin)
        {
            const double frequency=double(bin)*sample_rate/fft_size;
            if(frequency<40 || frequency>12000) continue;
            const unsigned band=frequency<250?0:(frequency<2000?1:2);
            const float magnitude=std::log1p(std::abs(spectrum[bin])*100.0f/float(fft_size));
            flux[band]+=std::max(0.0f,magnitude-previous_magnitudes[bin]);
            previous_magnitudes[bin]=magnitude; ++bins[band];
        }
        float novelty=0,raw=0;
        for(unsigned band=0;band<3;++band)
        {
            flux[band]/=float(std::max(1u,bins[band]));
            raw+=flux[band];
            const float normalized=std::max(0.0f,(flux[band]-1.15f*flux_mean[band])/
                                                (2.0f*flux_mean[band]+0.0025f));
            novelty=std::max(novelty,normalized);
            state.band_flux[band]=std::clamp(normalized,0.0f,1.0f);
            flux_mean[band]+=0.00995f*(flux[band]-flux_mean[band]);
        }
        // One cross-band peak per attack, one-hop confirmation, 90 ms refractory.
        float impulse=0;
        const double center_time=end_time-double(fft_size)/(2.0*sample_rate);
        if(previous_novelty>0.25f && previous_novelty>older_novelty && previous_novelty>=novelty &&
           previous_peak_time-last_event>=0.090 && rms>0.0003f)
        {
            last_event=previous_peak_time;
            state.last_onset_time=last_event; ++state.onset_sequence;
            state.onset_strength=std::clamp(previous_novelty,0.0f,1.0f);
            onset_peak=std::max(previous_raw,onset_peak*0.98f);
            impulse=std::clamp(previous_raw/std::max(onset_peak,0.00001f),0.02f,1.0f);
            events[event_next]={last_event,impulse};
            event_next=(event_next+1)%MaxEvents; event_count=std::min(event_count+1,MaxEvents);
        }
        older_novelty=previous_novelty; previous_novelty=novelty;
        previous_raw=raw; previous_peak_time=center_time;
        history[history_next]=impulse; history_next=(history_next+1)%History;
        history_count=std::min(history_count+1,History);
        if(estimate_countdown==0) { Estimate(end_time); estimate_countdown=24; }
        else --estimate_countdown;
        if(state.locked)
        {
            const double period=60.0/state.bpm;
            // Never continue a metronome indefinitely over a sustained note.
            if(end_time-last_event>std::max(1.0,period*2.2))
            { state.locked=false; state.confidence=state.phase=0; }
            else
            {
                const double phase=(end_time-phase_origin)/period;
                state.phase=float(phase-std::floor(phase));
            }
        }
    }

    float HistoryAt(std::size_t age) const
    {
        return age<history_count?history[(history_next+History-1-age)%History]:0;
    }

    float Correlation(unsigned lag) const
    {
        if(history_count<=lag+30) return 0;
        double numerator=0,a2=0,b2=0;
        // +/- one hop accommodates 90 BPM and attack location quantization.
        for(std::size_t age=0;age+lag+1<history_count;++age)
        {
            const double a=HistoryAt(age);
            const double b=std::max({HistoryAt(age+lag-1),HistoryAt(age+lag),HistoryAt(age+lag+1)});
            const double weight=std::exp(-double(age)/300.0);
            numerator+=weight*a*b; a2+=weight*a*a; b2+=weight*b*b/3.0;
        }
        return float(numerator/std::sqrt(std::max(a2*b2,1e-12)));
    }

    void Estimate(double time)
    {
        if(history_count<200 || event_count<6) return;
        float best=0; unsigned best_lag=0;
        std::array<float,101> scores{};
        for(unsigned lag=33;lag<=100;++lag)
        {
            const double period=std::max(1.0/3.0,double(lag)/100.0);
            double x=0,y=0,total=0;
            for(std::size_t n=0;n<event_count;++n)
            {
                const auto& event=events[(event_next+MaxEvents-1-n)%MaxEvents];
                const double age=time-event.time;
                if(age>6.0) break;
                const double weight=event.weight*event.weight*std::exp(-age/3.0);
                const double angle=Tau*event.time/period;
                x+=weight*std::cos(angle);y+=weight*std::sin(angle);total+=weight;
            }
            const float concentration=float(std::sqrt(x*x+y*y)/std::max(total,1e-12));
            // Score phase for EACH candidate: a strong three-beat correlation
            // must not hide the correct faster grid by winning before gating.
            scores[lag]=(0.6f*Correlation(lag)+0.25f*Correlation(lag*2)+0.15f*Correlation(lag*3))
                        *(0.25f+0.75f*concentration);
            // Prefer the fundamental interval over an equally supported multiple.
            if(scores[lag]>best*1.015f) {best=scores[lag];best_lag=lag;}
        }
        if(!best_lag || best<0.25f) {state.locked=false;state.confidence=state.phase=0;return;}
        // Alternating strong/weak beats otherwise favor half-time correlation.
        // Prefer the shorter grid only when its actual attacks also support it;
        // the phase-coherence gate below must still pass. This is an explicit
        // metrical ambiguity heuristic, not proof of a musical beat level.
        if(best_lag>=66)
        {
            const unsigned half=(best_lag+1)/2;
            if(scores[half]>=best*0.80f) {best_lag=half;best=scores[half];}
        }
        // Keep an already coherent interpretation across small estimation jitter.
        if(candidate_period>0)
        {
            const unsigned old=unsigned(std::round(candidate_period*100));
            if(old>=33 && old<=100 && scores[old]>=best*0.96f) best_lag=old;
        }
        const float period=std::max(1.0f/3.0f,best_lag/100.0f);
        if(std::abs(period-candidate_period)<=0.025f) ++candidate_repeats;
        else {candidate_repeats=1;candidate_period=period;}
        double x=0,y=0,total=0,oldest=time; unsigned recent=0;
        for(std::size_t n=0;n<event_count;++n)
        {
            const auto& event=events[(event_next+MaxEvents-1-n)%MaxEvents];
            const double age=time-event.time;
            if(age>6.0) break;
            const double weight=event.weight*event.weight*std::exp(-age/3.0);
            const double angle=Tau*event.time/period;
            x+=weight*std::cos(angle);y+=weight*std::sin(angle);total+=weight;
            oldest=event.time;++recent;
        }
        const double concentration=std::sqrt(x*x+y*y)/std::max(total,1e-12);
        const float evidence=float(std::min(1.0,(time-oldest)/3.0));
        state.confidence=std::clamp((best-0.20f)/0.65f,0.0f,1.0f)*float(concentration)*evidence;
        state.locked=recent>=6 && candidate_repeats>=3 && state.confidence>=0.50f &&
                     time-last_event<std::max(1.0,double(period)*2.2);
        if(state.locked)
        {
            state.bpm=60.0f/period;
            const double phase_angle=std::atan2(y,x)/Tau;
            phase_origin=phase_angle*period;
        }
        else state.phase=0;
    }
};
}
