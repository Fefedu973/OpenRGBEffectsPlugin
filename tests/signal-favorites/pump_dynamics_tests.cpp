// SPDX-License-Identifier: GPL-2.0-or-later
#include "PumpDynamics.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
static unsigned checks=0;
static void Check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
static void Near(double a,double b,double e,const char* message){Check(std::abs(a-b)<=e,message);}
static room_audio::RhythmSnapshot Tone(unsigned rate,double hz,double amplitude)
{
    room_audio::RhythmTracker tracker;std::vector<float> packet;
    unsigned sent=0;const unsigned total=rate/2;
    while(sent<total)
    {
        const unsigned n=std::min(total-sent,1+(sent*13+17)%719);
        packet.resize(n);for(unsigned i=0;i<n;++i)packet[i]=float(amplitude*std::sin(6.283185307179586*hz*(sent+i)/rate));
        Check(tracker.Push(packet.data(),n,rate,5.0+double(sent)/rate),"fragmented PCM accepted");sent+=n;
    }
    return tracker.Snapshot();
}
int main()
{
 try
 {
    for(unsigned rate:{44100u,48000u})for(unsigned bin:{1u,2u,20u,73u,150u,199u})
    {
        const auto s=Tone(rate,bin*50.0,.5);Near(s.power,.125,.01,"unweighted mono mean-square power");
        Check(s.spectrum[bin]>.3f && s.spectrum[bin]<.51f,"known sine reaches its independent 50Hz spectrum bin");
        auto peak=std::max_element(s.spectrum.begin(),s.spectrum.end())-s.spectrum.begin();
        Check(unsigned(peak)==bin,"frequency peak has correct index (not 64 repeated bins)");
        for(float v:s.spectrum)Check(std::isfinite(v)&&v>=0&&v<=1,"finite normalized magnitude");
    }
    auto low=Tone(8000,1000,.5);for(unsigned i=80;i<200;++i)Near(low.spectrum[i],0,0,"beyond Nyquist is zero");
    auto silence=Tone(48000,1000,0);Near(silence.power,0,0,"silent power zero");for(float v:silence.spectrum)Near(v,0,0,"silent spectrum zero");
    room_audio::RhythmSnapshot input;input.generation=1;input.power=.25f;input.spectrum.fill(.2f);
    native_pump::State state;auto a=state.Update({},1.0/60,input);
    Near(a.levels[0],.6,1e-6,"volume is 2.4*power at default boost");
    Near(a.frequencies[20],(1-std::exp(-1.2))*.6,1e-6,"default center spectrum is volume scaled");
    Near(a.levels[2],1.1*(1-std::exp(-1.2)),1e-6,"bass uses two positive bass bins");
    auto unscaled=state.Update({{"volumeScaleCenter",false}},0,input);
    Near(unscaled.frequencies[20],1-std::exp(-1.2),1e-6,"center volume toggle");
    input.power=0;input.spectrum.fill(0);auto decayed=state.Update({},.1,input);
    Near(decayed.levels[0],a.levels[0]*std::exp(-.1*40/30),1e-6,"volume exponential decay");
    Near(decayed.levels[3],a.levels[3]*std::exp(-.2*40/30),1e-6,"bass rectangle independent double-rate decay");
    for(float v:decayed.frequencies)Near(v,0,0,"measured silence clears current spectrum");
    native_pump::State helper;const nlohmann::json params={{"displayLayoutHelper",true}};
    auto h=helper.Update(params,.1,{});Near(h.levels[0],.98,1e-6,"helper volume descends");Near(h.levels[2],.02,1e-6,"helper bass ascends");
    auto paused=helper.Update(params,.1,{},true);for(unsigned i=0;i<8;++i)paused=helper.Update(params,.1,{});
    Near(paused.state[3],h.state[3],1e-6,"one tap pauses helper persistently");
    auto resumed=helper.Update(params,.1,{},true);Check(resumed.state[3]>paused.state[3],"second tap resumes helper");
    native_pump::State beat;room_audio::RhythmSnapshot pulse;pulse.generation=1;
    auto p=beat.Update({{"colorStyle","RandomBeat"},{"beatSensitivity",100}},.1,pulse);
    for(int i=0;i<8;++i)p=beat.Update({{"colorStyle","RandomBeat"},{"beatSensitivity",100}},.02,pulse);
    pulse.power=.2f;pulse.spectrum[0]=pulse.spectrum[1]=pulse.spectrum[2]=.8f;
    const float previous=p.state[1];p=beat.Update({{"colorStyle","RandomBeat"},{"beatSensitivity",100}},.02,pulse);
    Check(std::abs(p.state[1]-previous)>=.15,"measured bass attack causes separated random color");Near(p.state[2],1,1e-6,"actual attack starts flash");
    const float next=p.state[1];p=beat.Update({{"colorStyle","RandomBeat"},{"beatSensitivity",100}},.02,pulse);
    Near(p.state[1],next,0,"refractory prevents per-frame random strobe");
    pulse={};pulse.generation=2;p=beat.Update({},.02,pulse);Near(p.state[2],0,0,"capture generation change cancels old flash");
    pulse.power=std::numeric_limits<float>::quiet_NaN();pulse.spectrum.fill(std::numeric_limits<float>::infinity());
    p=beat.Update({{"usedFreqSector",500}},std::numeric_limits<double>::infinity(),pulse);
    Check(p.count==100,"frequency count bounded");for(float v:p.frequencies)Check(std::isfinite(v),"hostile audio sanitized");
    state.Reset();a=state.Update({},0,{});for(float v:a.levels)Near(v,0,0,"restart clears envelopes");
    std::cout<<"PASS "<<checks<<" spectrum and Pump dynamics checks\n";return 0;
 }
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
