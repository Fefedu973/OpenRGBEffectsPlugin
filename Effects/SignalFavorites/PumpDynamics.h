// SPDX-License-Identifier: GPL-2.0-or-later
// Original scalar audio dynamics for the native Pump Up Beats visualizer.
#pragma once
#include "RhythmTracker.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>

namespace native_pump
{
struct Frame
{
    std::array<float,4> levels{}; // volume bar/rectangle, bass bar/rectangle
    std::array<float,4> state{};  // cyclic hue, beat hue, flash, helper phase
    std::array<float,100> frequencies{};
    unsigned count=50;
};

class State
{
public:
    void Reset() { *this=State{}; }
    Frame Update(const nlohmann::json& values, double elapsed,
                 const room_audio::RhythmSnapshot& audio, bool helperTap=false)
    {
        const auto empty=nlohmann::json::object();
        const auto& p=values.is_object()?values:empty;
        const double dt=std::isfinite(elapsed)?std::clamp(elapsed,0.0,.25):0;
        time+=dt;
        hue=Fraction(hue+dt*Number(p,"huespeed",20,0,100)/200.0);
        if(helperTap) helper_running=!helper_running;
        // A lost endpoint must not replay an old attack or leave an old energy
        // baseline in the detector. Visual decay itself remains continuous.
        if(generation!=audio.generation)
        {generation=audio.generation; energies.clear(); pending_beat=false; last_beat=-1000;}
        levels[0]*=std::exp(-dt*Number(p,"volumeBarSmoothing",40,1,100)/30);
        levels[1]*=std::exp(-dt*Number(p,"volumeRectSmoothing",25,1,100)/30);
        levels[2]*=std::exp(-2*dt*Number(p,"baseBarSmoothing",50,1,100)/30);
        levels[3]*=std::exp(-2*dt*Number(p,"baseRectSmoothing",40,1,100)/30);
        const double boost=std::pow(6.0,Number(p,"totalVolumeBoost",50,0,100)/20-2.5);
        const double power=FiniteUnit(audio.power);
        const double volume=std::min(1.0,2.4*power*boost);
        levels[0]=std::max(levels[0],volume);levels[1]=std::max(levels[1],volume);
        const bool audible=2.4*power*200>=.0005;
        double bass=0;
        for(unsigned i=1;i<=2;++i) bass+=.55*(1-std::exp(-6*FiniteUnit(audio.spectrum[i])));
        bass=audible?std::min(1.0,bass):0;
        levels[2]=std::max(levels[2],bass);levels[3]=std::max(levels[3],bass);

        const double a=FiniteUnit(audio.spectrum[0]),b=FiniteUnit(audio.spectrum[1]);
        const double energy=audible?a*a+b*b:0;
        energies.push_back({time,energy});
        while(energies.size()>128 || (energies.size()>1 && time-energies.front().time>=.3)) energies.pop_front();
        double average=0,variance=0;
        for(const auto& e:energies) average+=e.value/energies.size();
        if(energies.size()>=3 && average>1e-12)
        {
            for(std::size_t i=0;i+1<energies.size();++i)
            {const double d=(average-energies[i].value)/average;variance+=d*d/(energies.size()-2);}
            const double threshold=1.95*(1+3*variance)*(2-Number(p,"beatSensitivity",50,1,100)/50)*average;
            if(energy>threshold && bass>.2 && time-last_beat>.1)
            {pending_beat=true;last_beat=time;}
        }
        const bool uses_random=p.value("colorStyle",std::string("HueCycle"))=="RandomBeat" ||
            p.value("backgroundStyle",std::string("Static"))=="RandomBeat" ||
            p.value("backgroundMode",std::string("Off"))=="RandomBeat";
        if(pending_beat && uses_random)
        {
            double next=Random();
            // Bounded replacement for rejection sampling, same minimum hue jump.
            if(std::abs(next-random_hue)<.15) next=Fraction(random_hue+.5);
            random_hue=next;pending_beat=false;
        }
        Frame result;result.count=unsigned(Number(p,"usedFreqSector",50,10,100));
        const bool scale=Boolean(p,"volumeScaleCenter",true);
        for(unsigned i=0;i<result.count;++i)
            result.frequencies[i]=float(audible?(1-std::exp(-6*FiniteUnit(audio.spectrum[i])))*(scale?levels[0]:1):0);
        if(Boolean(p,"displayLayoutHelper",false))
        {
            if(helper_running) helper_phase=Fraction(helper_phase+dt/10);
            const double v=std::abs(2*helper_phase-1);
            levels={v,v,1-v,1-v};
            for(unsigned i=0;i<result.count;++i)
            {const double x=double(i)/(result.count-1);result.frequencies[i]=float(v*(std::exp(-4*x)+x*std::exp(-8*std::abs(x-.6))));}
        }
        const double fade=std::pow((101-Number(p,"baseRectSmoothing",40,1,100))/100,2);
        for(unsigned i=0;i<4;++i) result.levels[i]=float(levels[i]);
        result.state={float(hue),float(random_hue),float(std::clamp(1-(time-last_beat)/fade,0.0,1.0)),float(helper_phase)};
        return result;
    }
private:
    struct Energy {double time,value;};
    std::deque<Energy> energies;
    std::array<double,4> levels{};
    std::uint64_t generation=0;
    std::uint32_t seed=0xfacba731;
    double time=0,hue=0,random_hue=.47,last_beat=-1000,helper_phase=0;
    bool helper_running=true,pending_beat=false;
    static double Fraction(double v) {return v-std::floor(v);}
    static double FiniteUnit(float v) {return std::isfinite(v)?std::clamp(double(v),0.0,1.0):0;}
    static double Number(const nlohmann::json& p,const char* key,double fallback,double low,double high)
    {auto it=p.find(key);if(it==p.end()||!it->is_number())return fallback;double v=it->get<double>();return std::isfinite(v)?std::clamp(v,low,high):fallback;}
    static bool Boolean(const nlohmann::json& p,const char* key,bool fallback)
    {auto it=p.find(key);return it!=p.end()&&it->is_boolean()?it->get<bool>():fallback;}
    double Random() {seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return double(seed&0xffffffu)/16777216;}
};
}
