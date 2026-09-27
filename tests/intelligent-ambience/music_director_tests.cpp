// SPDX-License-Identifier: GPL-2.0-or-later
// Synthetic existing-tracker observations only: no capture, Qt or devices.
#include "Effects/IntelligentAmbience/MusicDirector.h"
#include <atomic>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using room_ai::MusicDirector;
using room_audio::RhythmSnapshot;
namespace
{
unsigned checks=0;
void Check(bool condition,const char* name)
{++checks;if(!condition)throw std::runtime_error(name);}
bool Dark(room_ai::Color c){return c.r==0&&c.g==0&&c.b==0;}
bool Finite(room_ai::Color c)
{return std::isfinite(c.r)&&std::isfinite(c.g)&&std::isfinite(c.b)&&c.r>=0&&c.r<=1&&c.g>=0&&c.g<=1&&c.b>=0&&c.b<=1;}
RhythmSnapshot Input(double elapsed=0)
{
    RhythmSnapshot r;r.generation=1;r.sequence=1+std::uint64_t(std::round(elapsed*100));
    r.audio_time=1000+elapsed;r.power=.09f;r.silent=false;r.confidence=.12f;
    r.spectrum[4]=.16f;r.spectrum[12]=.36f;r.spectrum[155]=.81f;
    return r;
}
void InitialAndValidation()
{
    MusicDirector d;
    Check(!d.Snapshot(1000).valid,"initial snapshot invalid");
    Check(Dark(d.Sample({.3,.2},1000,true)),"initial output black");
    auto r=Input();d.Push(r,1000);
    auto s=d.Snapshot(1000);
    Check(s.valid&&s.scene=="Calm","audible warmup is calm");
    Check(s.tempo_bpm==0&&!s.beat,"warmup fabricates no BPM or accent");
    Check(std::abs(s.confidence-.12)<1e-6,"confidence is original score, not forced lock");
    Check(std::abs(s.level-.3)<1e-6,"RMS uses original power");
    Check(s.bands[1]==.16f&&s.bands[3]==.36f&&s.bands[7]==.81f,"eight frequency bands retain source values");
    Check(!Dark(d.Sample({.3,.2},1000,true)),"audible composition produces color");
    Check(Dark(d.Sample({.3,.2},1000.151,true)),"stale composition black");
    Check(!d.Snapshot(999.99).valid,"clock before observation rejected");
    r=Input(.02);r.spectrum[25]=std::numeric_limits<float>::quiet_NaN();d.Push(r,r.audio_time);
    Check(!d.Snapshot(r.audio_time).valid,"nonfinite spectrum rejected");
    r=Input(.03);r.power=-.1f;d.Push(r,r.audio_time);
    Check(!d.Snapshot(r.audio_time).valid,"negative power rejected");
    r=Input(.04);d.Push(r,std::numeric_limits<double>::infinity());
    Check(!d.Snapshot(r.audio_time).valid,"infinite render clock rejected");
    r=Input(.05);d.Push(r,r.audio_time-.01);
    Check(!d.RenderState(r.audio_time).valid,"future input never remapped to now");
    r=Input(.06);r.sequence=0;d.Push(r,r.audio_time);
    Check(!d.RenderState(r.audio_time).valid,"uninitialized tracker rejected");
    r=Input(.07);d.Push(r,r.audio_time);
    r=Input(.08);r.onset_sequence=1;r.last_onset_time=r.audio_time+.001;r.onset_strength=1;
    d.Push(r,r.audio_time+.01);
    Check(!d.Snapshot(r.audio_time+.01).beat,"onset newer than its observation cannot produce accent");
}
void LockSilenceAndGeneration()
{
    MusicDirector d;auto r=Input();r.locked=true;r.confidence=.85f;r.bpm=120;r.phase=.95f;
    d.Push(r,r.audio_time);
    auto s=d.Snapshot(1000.05);
    Check(s.tempo_bpm==120&&std::abs(s.phase-.05)<1e-6,"phase advances using audio time and wraps");
    Check(!s.beat,"acquiring a tempo lock creates no fake accent");
    r=Input(.06);r.locked=true;r.bpm=120;r.confidence=.3f;d.Push(r,r.audio_time);
    Check(d.Snapshot(r.audio_time).tempo_bpm==0,"uncertain lock has no BPM");
    Check(std::abs(d.Snapshot(r.audio_time).confidence-.3)<1e-6,"uncertain confidence preserved");
    r=Input(.07);r.locked=true;r.bpm=181;r.confidence=1;d.Push(r,r.audio_time);
    Check(d.Snapshot(r.audio_time).tempo_bpm==0,"out of range tempo rejected");
    r=Input(.08);r.onset_sequence=1;r.last_onset_time=r.audio_time;r.onset_strength=.9f;
    d.Push(r,r.audio_time);Check(d.Snapshot(r.audio_time).beat,"new measured onset selects accent");
    const auto before=d.RenderState(r.audio_time);
    d.Push(r,r.audio_time+.01);const auto after=d.RenderState(r.audio_time+.01);
    Check(after.accents[0][2]<before.accents[0][2],"duplicate sequence does not retrigger accent");
    r.silent=true;r.locked=false;d.Push(r,r.audio_time+.02);
    s=d.Snapshot(r.audio_time+.02);
    Check(s.valid&&s.scene=="Silence"&&!s.beat&&s.tempo_bpm==0,"same-sequence no-packet silence clears beat");
    Check(Dark(d.Sample({.3,.2},r.audio_time+.02,true)),"same-sequence silence immediately dark");
    r=Input(.12);r.generation=2;r.onset_sequence=400;r.last_onset_time=r.audio_time;r.onset_strength=1;
    d.Push(r,r.audio_time);s=d.Snapshot(r.audio_time);
    Check(s.valid&&!s.beat&&s.scene=="Calm","generation change does not replay inherited onset counter");
    d.Reset();Check(!d.Snapshot(r.audio_time).valid,"Reset discards snapshot");
    Check(Dark(d.Sample({.3,.2},r.audio_time,true)),"Reset clears output");
}
void AccentFreshnessBudget()
{
    MusicDirector d;auto r=Input();d.Push(r,r.audio_time);
    r=Input(.01);r.onset_sequence=1;r.last_onset_time=999;r.onset_strength=1;
    d.Push(r,r.audio_time);Check(!d.Snapshot(r.audio_time).beat,"old onset skipped even on counter change");
    Check(d.Snapshot(r.audio_time).onset==0,"old onset intensity suppressed");
    unsigned selected=0;
    for(unsigned i=2;i<400;++i)
    {
        r=Input(i*.01);r.onset_sequence=i;r.last_onset_time=r.audio_time;r.onset_strength=1;
        d.Push(r,r.audio_time);
        if(d.Snapshot(r.audio_time).reason.find("Selected transient")!=std::string::npos)++selected;
    }
    Check(selected==4,"at most four selected accents in first four seconds");
    r=Input(4.03);r.onset_sequence=403;r.last_onset_time=r.audio_time;r.onset_strength=1;
    d.Push(r,r.audio_time);Check(d.Snapshot(r.audio_time).beat,"accent budget renews after elapsed window");
    r=Input(4.04);r.onset_sequence=404;r.last_onset_time=r.audio_time;r.onset_strength=1;
    d.Push(r,r.audio_time);
    Check(d.Snapshot(r.audio_time).reason.find("ignored")!=std::string::npos,"refractory prevents rapid retrigger");
    r=Input(6);r.onset_sequence=900;r.last_onset_time=r.audio_time;r.onset_strength=1;
    d.Push(r,r.audio_time);
    Check(!d.Snapshot(r.audio_time).beat,"render pause discards past rhythm/accent ownership");
    Check(d.Snapshot(r.audio_time).scene=="Calm","long render gap restarts conservative scene");
    // A sequence counter reset without an epoch change is still a discontinuity.
    r.sequence=1;r.audio_time=1006.01;d.Push(r,r.audio_time);
    Check(!d.Snapshot(r.audio_time).beat,"sequence regression cannot retrigger");
}
void ScenesAndCadences()
{
    for(unsigned fps:{30u,60u})
    {
        MusicDirector d;double previous_onset=-1;
        for(unsigned i=0;i<=fps*12;++i)
        {
            const double t=double(i)/fps;auto r=Input(t);
            r.locked=true;r.bpm=120;r.phase=float(std::fmod(t*2,1.));r.confidence=.9f;
            const double onset_time=std::floor(t*2)/2;
            r.onset_sequence=std::uint64_t(onset_time*2)+1;r.last_onset_time=1000+onset_time;
            r.onset_strength=.6f;d.Push(r,r.audio_time);
            if(t<7.9)Check(d.Snapshot(r.audio_time).scene=="Calm","scene has minimum dwell");
            if(onset_time!=previous_onset)previous_onset=onset_time;
        }
        const auto s=d.Snapshot(1012);Check(s.scene=="Groove","stable tracker evidence reaches Groove at either render cadence");
        Check(s.tempo_bpm==120,"tempo not estimated a second time");
        Check(Finite(d.Sample({.4,.2},1012,true)),"steady directed sample bounded");
    }
    MusicDirector d;
    for(unsigned i=0;i<=1500;++i){auto r=Input(i*.01);d.Push(r,r.audio_time);}
    auto s=d.Snapshot(1015);
    Check(s.scene=="Calm"&&s.tempo_bpm==0&&!s.beat,"steady energy alone never invents beat grid");
    auto r=Input(15.01);r.silent=true;d.Push(r,r.audio_time);
    Check(d.Snapshot(r.audio_time).scene=="Silence","silence overrides minimum scene duration");
    d.Reset();double t=0;unsigned index=0;
    const double intervals[]={.016,.050,.023,.090,.034};
    while(t<12)
    {
        r=Input(t);r.locked=true;r.bpm=90;r.confidence=.8f;r.phase=float(std::fmod(t*1.5,1.));
        r.onset_sequence=1+std::uint64_t(t*1.5);r.last_onset_time=1000+std::floor(t*1.5)/1.5;r.onset_strength=.7f;
        d.Push(r,r.audio_time);t+=intervals[index++%5];
    }
    s=d.Snapshot(r.audio_time);
    Check(s.valid&&s.scene=="Groove"&&s.tempo_bpm==90,"irregular render cadence keeps original audio grid");
}
void RenderAndIntensity()
{
    MusicDirector d;auto r=Input();d.Push(r,r.audio_time);
    auto state=d.RenderState(1000);
    Check(state.valid&&state.controls[1]==.6f,"render packet initialized");
    Check(state.accents.size()==16&&state.bands.size()==8,"uniform payload bounded");
    d.SetIntensity(0);Check(Dark(d.Sample({.2,.2},1000,true)),"intensity zero is black");
    d.SetIntensity(2);Check(d.RenderState(1000).controls[1]==1,"intensity clamped");
    d.SetIntensity(std::numeric_limits<double>::quiet_NaN());
    Check(Dark(d.Sample({.2,.2},1000,true)),"nonfinite intensity fails dark");
    d.SetIntensity(.4);d.Reset();r=Input(.1);d.Push(r,r.audio_time);
    Check(std::abs(d.RenderState(r.audio_time).controls[1]-.4)<1e-6,"Reset preserves user intensity");
    Check(Dark(d.Sample({std::numeric_limits<double>::quiet_NaN(),0},r.audio_time,true)),"invalid position rejected");
    Check(Dark(d.Sample({.9,.01},r.audio_time,false)),"reference band clips above bar");
    Check(!Dark(d.Sample({.9,.5},r.audio_time,false)),"reference bars use measured spectrum");
    Check(Finite(d.Sample({-10,4},r.audio_time,true)),"offscreen world positions stay finite");
    Check(!d.RenderState(r.audio_time+.2).valid,"stale packet has no valid shader state");
    Check(d.RenderState(r.audio_time+.2).controls[1]==0,"stale shader packet completely dark");
}
void Publication()
{
    MusicDirector d;std::atomic<bool> running{true},safe{true};
    std::thread reader([&]{while(running){const auto c=d.Sample({.2,.2},1000,true);if(!Finite(c))safe=false;d.Snapshot(1000);}});
    for(unsigned i=0;i<1000;++i){auto r=Input(i*.01);d.Push(r,r.audio_time);if(i%100==0)d.Reset();}
    running=false;reader.join();Check(safe,"concurrent value snapshot publication stays finite");
}
}
int main()
{
    try{InitialAndValidation();LockSilenceAndGeneration();AccentFreshnessBudget();ScenesAndCadences();RenderAndIntensity();Publication();
        std::cout<<"MusicDirector: "<<checks<<" assertions passed (synthetic RhythmSnapshot, no capture)\n";return 0;}
    catch(const std::exception& e){std::cerr<<"MusicDirector: "<<e.what()<<" after "<<checks<<" assertions\n";return 1;}
}
