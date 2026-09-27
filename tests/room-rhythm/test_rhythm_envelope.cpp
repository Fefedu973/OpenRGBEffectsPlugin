// SPDX-License-Identifier: GPL-2.0-or-later
#include "Effects/Shaders/RhythmEnvelope.h"
#include "Effects/Shaders/MusicEnvelope.h"
#include <iostream>
#include <stdexcept>
#include <vector>

static unsigned checks=0;
static void Check(bool pass,const char* message){++checks;if(!pass)throw std::runtime_error(message);}
static room_audio::RhythmSnapshot Frame(double time,bool locked=true)
{
    room_audio::RhythmSnapshot s;
    s.generation=1;s.sequence=1+std::uint64_t(time*100);s.audio_time=time;
    s.silent=false;s.locked=locked;s.bpm=120;s.confidence=locked?.9f:0;
    s.phase=float(std::fmod(time*2,1.0));return s;
}
static std::vector<double> Pulses(unsigned fps,bool jitter=false)
{
    RhythmEnvelope envelope;
    std::vector<double> pulses;
    unsigned step=0;
    for(double time=.13;time<5;)
    {
        const auto output=envelope.Update(Frame(time),time);
        if(output.rhythm[3]>.999f)pulses.push_back(time);
        time+=jitter?(step++%3==0?.011:(step%3==1?.071:.025)):1.0/fps;
    }
    return pulses;
}
int main()
{
    try
    {
        for(unsigned fps:{30u,60u})
        {
            const auto pulses=Pulses(fps);
            Check(pulses.size()==9,"regular renderer pulse count differs from beat grid");
            for(std::size_t i=0;i<pulses.size();++i)
                Check(pulses[i]>=(i+1)*.5 && pulses[i]<(i+1)*.5+1.0/fps+.0001,"beat outside one render interval");
        }
        const auto jitter=Pulses(30,true);
        Check(jitter.size()==9,"irregular renderer lost/doubled beats");
        for(std::size_t i=0;i<jitter.size();++i)
            Check(jitter[i]>=(i+1)*.5 && jitter[i]<(i+1)*.5+.072,"irregular pulse not audio-phase aligned");
        {
            RhythmEnvelope envelope;
            auto s=Frame(10);
            auto value=envelope.Update(s,10);
            Check(value.rhythm[3]==0,"lock acquisition invented a beat");
            s=Frame(10.4,false);value=envelope.Update(s,10.4);
            Check(value.rhythm[0]==0 && value.rhythm[3]==0,"uncertain state predicted a beat");
            s.onset_sequence=1;s.last_onset_time=10.41;s.audio_time=10.42;
            value=envelope.Update(s,10.42);
            Check(value.rhythm[3]==1 && value.transients[3]==1,"uncertain transient did not remain usable");
            const float hue=value.hue;
            value=envelope.Update(s,10.44);
            Check(value.rhythm[3]<1 && value.transients[3]<1 && value.hue==hue,"same onset replayed each render frame");
            s=Frame(10.45);s.onset_sequence=1;value=envelope.Update(s,10.45);
            Check(value.rhythm[3]<1,"reacquisition created a new pulse");
            s=Frame(10.51);s.onset_sequence=1;value=envelope.Update(s,10.51);
            Check(value.rhythm[3]==1,"first real wrap after acquisition missed");
            s.silent=true;value=envelope.Update(s,10.52);
            Check(value.rhythm==std::array<float,4>{} && value.transients==std::array<float,4>{},"silence kept envelopes alive");
            s=Frame(11);s.generation=2;s.onset_sequence=999;value=envelope.Update(s,11);
            Check(value.rhythm[3]==0 && value.transients[3]==0,"generation reset replayed old onset");
            value=envelope.Update(s,11.151);
            Check(value.rhythm==std::array<float,4>{},"stale data predicted beats");
            s=Frame(11.2);s.generation=2;value=envelope.Update(s,11.01);
            Check(value.rhythm==std::array<float,4>{},"future timestamps passed freshness gate");
            s=Frame(9);s.generation=2;value=envelope.Update(s,9);
            Check(value.rhythm[3]==0,"backward render clock invented a pulse");
        }
        {
            RhythmEnvelope envelope;
            auto s=Frame(1.1,false);s.band_flux={1,.5f,.25f};
            auto value=envelope.Update(s,1.1);
            Check(value.transients[0]>.2 && value.transients[1]>value.transients[2],"three bands not preserved");
            s=Frame(1.15,false);value=envelope.Update(s,1.15);
            Check(value.transients[0]>0 && value.transients[0]<.25,"band decay not time based");
        }
        {
            RhythmEnvelope envelope;
            auto s=Frame(10,false);
            envelope.Update(s,10);
            s=Frame(10.31,false);s.onset_sequence=1;s.last_onset_time=10.02;
            auto value=envelope.Update(s,10.31);
            Check(value.transients[3]==0 && value.rhythm[3]==0,"render stall replayed a stale onset");
            s=Frame(10.33,false);s.onset_sequence=2;s.last_onset_time=10.32;
            value=envelope.Update(s,10.33);
            Check(value.transients[3]==1 && value.rhythm[3]==1,"fresh onset lost after stale event was consumed");
        }
        {
            // Pure legacy envelope remains independently usable; the renderer's
            // null-Rhythm branch is additionally checked by run_envelope.py.
            MusicEnvelope legacy;
            std::array<float,256> spectrum{};
            legacy.Update(spectrum.data(),0);
            spectrum[4]=spectrum[8]=spectrum[12]=.8f;
            const auto result=legacy.Update(spectrum.data(),.03);
            Check(result[0]>0 && result[1]>0 && result[3]==1,"legacy bass-envelope behavior changed");
        }
        std::cout<<"PASS "<<checks<<" rhythm-envelope assertions\n";
    }
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
