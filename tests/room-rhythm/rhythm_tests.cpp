// SPDX-License-Identifier: GPL-2.0-or-later
#include "Audio/RhythmTracker.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <functional>
#include <limits>
#include <memory>

static unsigned assertions=0;
static void Check(bool ok,const char* message) {++assertions;if(!ok)throw std::runtime_error(message);}
static constexpr double PI=3.14159265358979323846;
static float Hit(double t,double frequency,double amplitude)
{
    if(t<0 || t>.070)return 0;
    const double envelope=std::min(1.0,t/.002)*std::exp(-t/.012);
    return float(amplitude*envelope*std::sin(2*PI*frequency*t));
}
using Signal=std::function<float(double)>;
static Signal Pulses(double bpm,bool weak=false,bool syncopation=false,bool volume=false,double frequency=90)
{
    return [=](double t)
    {
        const double period=60.0/bpm;
        const int beat=int(std::floor((t-.20)/period));
        if(beat<0)return 0.f;
        const double age=t-.20-beat*period;
        double amplitude=weak && beat%2?0.20:0.65;
        if(volume)amplitude*=t<7?0.35:(t<13?1.0:0.5);
        float sample=Hit(age,frequency,amplitude);
        if(syncopation)
        {
            // Offbeat high-mid accents coexist with quieter on-beats; there is
            // no bass-only assumption and the estimator must not double tempo.
            sample+=Hit(age-period*.5,3100,amplitude*.25);
            if(beat%4==3)sample+=Hit(age-period*.75,900,amplitude*.20);
        }
        return sample;
    };
}
struct Result {room_audio::RhythmSnapshot last{}; std::vector<double> onsets; unsigned locked=0;};
static Result Feed(room_audio::RhythmTracker& tracker,const Signal& signal,unsigned rate,double seconds,
                   std::size_t chunk=317,double start=0)
{
    Result result; std::uint64_t observed=tracker.Snapshot().onset_sequence;
    const std::size_t length=std::size_t(seconds*rate);
    std::vector<float> data(chunk);
    for(std::size_t first=0;first<length;first+=chunk)
    {
        const std::size_t n=std::min(chunk,length-first);
        for(std::size_t i=0;i<n;++i)data[i]=signal(start+double(first+i)/rate);
        if(!tracker.Push(data.data(),n,rate,start+double(first)/rate))throw std::runtime_error("Push rejected valid PCM");
        result.last=tracker.Snapshot();
        if(result.last.onset_sequence!=observed)
        {result.onsets.push_back(result.last.last_onset_time);observed=result.last.onset_sequence;}
        if(result.last.locked)++result.locked;
    }
    return result;
}
static void Summary(const char* name,const Result& result)
{
    std::cout<<name<<": bpm="<<result.last.bpm<<", confidence="<<result.last.confidence
             <<", locked="<<result.last.locked<<", onsets="<<result.last.onset_sequence
             <<", phase="<<result.last.phase<<"\n";
}

int main()
{
    try
    {
        for(double bpm:{90.,120.,150.})
        {
            for(unsigned rate:{44100u,48000u})
            {
                auto tracker=std::make_unique<room_audio::RhythmTracker>();
                const auto result=Feed(*tracker,Pulses(bpm),rate,20);
                Summary(("pulses-"+std::to_string(int(bpm))+"-"+std::to_string(rate)).c_str(),result);
                Check(result.last.locked,"regular pulses did not lock");
                Check(std::abs(result.last.bpm-bpm)<2.0,"regular pulse tempo inaccurate");
                Check(result.last.confidence>.5,"regular pulse confidence low");
                Check(result.last.onset_sequence>=unsigned(19*bpm/60),"attacks lost from continuous PCM");
                Check(result.last.onset_sequence<=unsigned(20*bpm/60)+1,"duplicate attacks per pulse");
                double error=0;
                for(double t:result.onsets)
                {
                    const double beat=std::round((t-.2)*bpm/60);
                    error=std::max(error,std::abs(t-(.2+beat*60/bpm)));
                }
                Check(error<.045,"attack timestamp differs from fixture by more than45ms");
                const double expected=std::fmod((result.last.audio_time-.20)*bpm/60.0,1.0);
                const double difference=std::abs(result.last.phase-expected);
                Check(std::min(difference,1.0-difference)<.065,"locked phase not aligned with actual pulse timestamps");
            }
        }
        for(auto scenario:{std::string("weak"),std::string("syncopated"),std::string("volume"),std::string("high-only")})
        {
            auto tracker=std::make_unique<room_audio::RhythmTracker>();
            const auto result=Feed(*tracker,Pulses(120,scenario=="weak",scenario=="syncopated",scenario=="volume",scenario=="high-only"?5000:90),48000,24);
            Summary(scenario.c_str(),result);
            Check(result.last.locked,"mixed fixture did not lock");
            Check(std::abs(result.last.bpm-120)<2,"mixed fixture incorrect tempo");
        }
        for(double bpm:{157.,173.})
        {
            for(unsigned rate:{44100u,48000u})
            {
                // A clear kick grid with quiet sixteenth-note hats. At these
                // tempi the old 90ms global refractory discarded alternating
                // hats AND kicks after spectral-peak quantization, destroying
                // the otherwise stable beat evidence.
                const Signal mix=[=](double t)
                {
                    const double beat=std::fmod(t,60.0/bpm),sub=std::fmod(t,15.0/bpm);
                    const double kick=beat<.07?.8*std::exp(-beat/.018)*std::sin(2*PI*100*t):0;
                    const double hat=sub<.025?.2*std::exp(-sub/.008)*
                        (std::sin(2*PI*4200*t)+std::sin(2*PI*7100*t))*.5:0;
                    return float(kick+hat);
                };
                auto tracker=std::make_unique<room_audio::RhythmTracker>();
                Feed(*tracker,mix,rate,10,rate/100);
                const auto result=Feed(*tracker,mix,rate,20,rate/100,10);
                Summary(("sixteenth-hats-"+std::to_string(int(bpm))+"-"+std::to_string(rate)).c_str(),result);
                Check(result.last.locked && result.locked>=1900,"subdivisions suppress the underlying kick grid");
                Check(std::abs(result.last.bpm-bpm)<2,"subdivisions doubled or halved musical tempo");
                Check(result.last.onset_sequence>=unsigned(29*bpm/15),"legitimate sixteenth-note attacks discarded");
                Check(result.last.onset_sequence<=unsigned(30*bpm/15)+1,"subdivision attacks duplicated");
                const double expected=std::fmod(result.last.audio_time*bpm/60.0,1.0);
                const double error=std::abs(result.last.phase-expected);
                Check(std::min(error,1.0-error)<.15,"subdivision beat phase lost alignment");
            }
        }
        {
            auto tracker=std::make_unique<room_audio::RhythmTracker>();
            auto silence=Feed(*tracker,[](double){return 0.f;},48000,5);
            Check(silence.last.silent && !silence.last.locked && silence.last.confidence==0 && silence.last.onset_sequence==0,"silence invented events");
            auto tone=Feed(*tracker,[](double t){return float(.25*std::sin(2*PI*1000*t));},48000,8,317,5);
            Summary("sustained tone",tone);
            Check(!tone.last.locked,"sustained tone invented tempo");
            Check(tone.last.onset_sequence<=2,"sustained tone repeatedly triggered");
        }
        {
            auto tracker=std::make_unique<room_audio::RhythmTracker>();
            Feed(*tracker,Pulses(120),48000,10);
            auto silence=Feed(*tracker,[](double){return 0.f;},48000,1.5,317,10);
            Check(!silence.last.locked && silence.last.confidence==0 && silence.last.phase==0,"pause retained predictive phase");
            auto resumed=Feed(*tracker,[](double t){return Pulses(150)(t-11.5);},48000,15,317,11.5);
            Summary("resume150",resumed);
            Check(resumed.last.locked && std::abs(resumed.last.bpm-150)<2,"resumed tempo not reacquired");
            float sample=0; const auto generation=tracker->Snapshot().generation;
            tracker->Push(&sample,1,48000,80.0);
            Check(tracker->Snapshot().generation==generation+1 && !tracker->Snapshot().locked,"gap did not reset");
            tracker->Push(&sample,1,44100,80.0,true);
            Check(tracker->Snapshot().generation==generation+2,"explicit discontinuity did not reset");
            tracker->Push(nullptr,0,44100,81.0,true);
            Check(tracker->Snapshot().generation==generation+3,"empty discontinuity did not reset");
            Check(!tracker->Push(nullptr,1,48000,0),"invalid pointer accepted");
            Check(!tracker->Push(&sample,1,0,0),"invalid sample rate accepted");
        }
        {
            auto tracker=std::make_unique<room_audio::RhythmTracker>();
            const auto result=Feed(*tracker,[](double t){return t<10?Pulses(120)(t):Pulses(150)(t-10);},48000,25);
            Summary("tempo120to150",result);
            Check(result.last.locked && std::abs(result.last.bpm-150)<2,"continuous tempo change not followed");
        }
        {
            std::vector<double> hits;
            std::uint32_t random=0x72687974u;
            for(double t=.2;t<20;)
            {
                hits.push_back(t);random=random*1664525u+1013904223u;
                t+=.18+.79*double(random>>8)/16777216.0;
            }
            auto tracker=std::make_unique<room_audio::RhythmTracker>();
            const auto result=Feed(*tracker,[&](double t)
            {
                float sample=0;for(double time:hits)sample+=Hit(t-time,900,.5);return sample;
            },48000,20);
            Summary("irregular",result);
            Check(result.last.onset_sequence>=20,"irregular attacks not exposed");
            Check(!result.last.locked,"irregular fixture incorrectly advertised reliable phase");
            Check(result.last.phase==0,"uncertain fixture advertised a predictive phase");
        }
        {
            auto tracker=std::make_unique<room_audio::RhythmTracker>();
            const auto result=Feed(*tracker,[](double t){return Pulses(120)(t)+Pulses(120,false,false,false,5000)(t);},48000,12);
            Check(result.last.onset_sequence==24,"same hit in bass and treble double-triggered");
        }
        {
            auto tracker=std::make_unique<room_audio::RhythmTracker>();
            const auto result=Feed(*tracker,[](double t){return float((t<4?.1:.5)*std::sin(2*PI*1000*t));},48000,8);
            Check(!result.last.locked && result.last.onset_sequence<=3,"one volume change invented a rhythm");
        }
        {
            auto tracker=std::make_unique<room_audio::RhythmTracker>();
            const auto result=Feed(*tracker,[](double t){return Pulses(120)(t-100000);},48000,12,317,100000);
            Summary("absoluteaudio",result);
            Check(result.last.locked && std::abs(result.last.bpm-120)<2,"absolute QPC-like timestamps broke tempo");
            Check(result.last.last_onset_time>100000,"absolute timestamp was discarded");
            const auto generation=tracker->Snapshot().generation;
            float sample=0;
            tracker->Push(&sample,1,48000,2);
            Check(tracker->Snapshot().generation==generation+1,"backward audio clock failed to reset");
        }
        {
            for(double bpm:{60.,180.})
            {
                auto tracker=std::make_unique<room_audio::RhythmTracker>();
                const auto result=Feed(*tracker,Pulses(bpm),48000,18);
                Summary(("boundary-"+std::to_string(int(bpm))).c_str(),result);
                Check(result.last.locked && std::abs(result.last.bpm-bpm)<2,"tempo boundary not supported");
                Check(result.last.bpm>=60 && result.last.bpm<=180,"tempo escaped promised range");
            }
            for(unsigned rate:{8000u,192000u})
            {
                auto tracker=std::make_unique<room_audio::RhythmTracker>();
                const auto result=Feed(*tracker,Pulses(120),rate,.8);
                Check(result.last.sequence>60 && result.last.sequence<81,"hop clock is not10ms at rate boundary");
                Check(!result.last.locked && result.last.bpm==0,"insufficient evidence manufactured a tempo");
            }
            auto tracker=std::make_unique<room_audio::RhythmTracker>();
            const auto result=Feed(*tracker,[](double){return std::numeric_limits<float>::quiet_NaN();},48000,1);
            Check(result.last.onset_sequence==0 && !result.last.locked,"nonfinite input created rhythm");
        }
        {
            auto a=std::make_unique<room_audio::RhythmTracker>();
            auto b=std::make_unique<room_audio::RhythmTracker>();
            const auto one=Feed(*a,Pulses(120,true,true),48000,15,137);
            const auto two=Feed(*b,Pulses(120,true,true),48000,15,8191);
            Check(one.last.onset_sequence==two.last.onset_sequence,"packetization changed onset count");
            Check(one.last.sequence==two.last.sequence,"packetization changed hop count");
            Check(one.last.bpm==two.last.bpm && one.last.confidence==two.last.confidence && one.last.phase==two.last.phase,"render/capture chunking changed rhythm");
            Check(sizeof(room_audio::RhythmTracker)<200000,"tracker memory unexpectedly unbounded");
        }
        std::cout<<"PASS "<<assertions<<" assertions\n";
        return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<"\n";return 1;}
}
