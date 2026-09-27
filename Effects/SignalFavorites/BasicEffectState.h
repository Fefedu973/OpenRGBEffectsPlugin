// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ShaderPass.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// Original, bounded per-effect simulations. Only numeric render state leaves
// this object: no browser, script interpreter, hardware, input capture or UI.
namespace native_basic
{
class State
{
public:
    void Reset() { *this=State{}; }
    static std::string Declarations(const std::string& id)
    {
        if(id=="ColorShift") return "uniform float bShiftCount;\nuniform vec4 bShift[10];\n";
        if(id=="PoliceLights") return "uniform float bPoliceTiming;\n";
        if(id=="QuadColorBreath") return "uniform float bBreathTint;\nuniform vec3 bQuadrant[4];\n";
        if(id=="CrookedWaves") return "uniform vec4 bBarPositions;\nuniform vec3 bBarColor;\n";
        return {};
    }
    ShaderUniformMap Update(const std::string& id,const nlohmann::json& parameters,double elapsed)
    {
        if(id!="ColorShift"&&id!="PoliceLights"&&id!="QuadColorBreath"&&id!="CrookedWaves") return {};
        const auto empty=nlohmann::json::object();
        const auto& p=parameters.is_object()?parameters:empty;
        if(active!=id) {Reset();active=id;}
        if(!started)
        {
            started=true;
            if(id=="ColorShift") shifts.push_back({Random(),0});
            if(id=="QuadColorBreath") LoadPalette(p);
            if(id=="CrookedWaves") {SetBars(p);bar_color=Color(p.value("color",std::string("#0a00ff")));}
        }
        // The caller caps UI stalls too. Local bound prevents arbitrary catch-up
        // loops even when this helper is exercised independently.
        if(std::isfinite(elapsed)&&elapsed>0) remainder+=std::min(elapsed,.25);
        const unsigned frames=unsigned(std::floor((remainder+1e-10)*60));
        remainder=std::max(0.0,remainder-frames/60.0);
        for(unsigned i=0;i<frames;++i)
        {
            if(id=="ColorShift")
            {
                const double speed=Number(p,"shiftSpeed",50,0,100);
                for(auto& s:shifts) s.alpha=std::min(1.0,s.alpha+speed/1000);
                if(shift_tick>=310-3*speed)
                {
                    shift_tick=0;
                    if(shifts.size()==10) shifts.erase(shifts.begin());
                    shifts.push_back({Random(),0});
                }
                else ++shift_tick;
            }
            else if(id=="PoliceLights")
            {
                const double step=std::floor(Number(p,"speed",50,0,100)/10+.5);
                police=police>0?police-step:100;
            }
            else if(id=="QuadColorBreath")
            {
                const bool rotate=p.value("rotate",true);
                if(!rotate) LoadPalette(p);
                if(tint<=0) {tint=0;rising=true;}
                else if(tint>=.95)
                {
                    tint=.95;rising=false;
                    if(rotate)std::rotate(palette.rbegin(),palette.rbegin()+1,palette.rend());
                }
                const double amount=.005*Number(p,"speed",1,1,5);
                tint+=rising?amount:-amount;
            }
            else
            {
                SetBars(p);
                if(!p.value("randomColors",false)) bar_color=Color(p.value("color",std::string("#0a00ff")));
                const double speed=Number(p,"speed",3,1,10),width=Number(p,"barWidth",30,1,50);
                for(unsigned bar=0;bar<bar_count;++bar)
                {
                    bars[bar]+=speed;
                    if(bars[bar]>=400-width)
                    {
                        bars[bar]=-50;
                        if(p.value("randomColors",false))bar_color={float(Random()),float(Random()),float(Random())};
                    }
                }
            }
        }
        // Static recoloring while rotation is disabled is immediately visible.
        if(id=="QuadColorBreath"&&!p.value("rotate",true)) LoadPalette(p);
        if(id=="CrookedWaves")
        {SetBars(p);if(!p.value("randomColors",false))bar_color=Color(p.value("color",std::string("#0a00ff")));}
        ShaderUniformMap result;
        if(id=="PoliceLights") result["bPoliceTiming"].values[0]=float(police);
        else if(id=="ColorShift")
        {
            result["bShiftCount"].values[0]=float(shifts.size());
            for(std::size_t i=0;i<shifts.size();++i)
                result["bShift["+std::to_string(i)+"]"]={{float(shifts[i].hue),float(shifts[i].alpha),0,0},4};
        }
        else if(id=="QuadColorBreath")
        {
            result["bBreathTint"].values[0]=float(std::clamp(tint,0.0,1.0));
            for(unsigned i=0;i<4;++i) result["bQuadrant["+std::to_string(i)+"]"]={{palette[i][0],palette[i][1],palette[i][2],0},3};
        }
        else
        {
            result["bBarPositions"]={{float(bars[0]),float(bars[1]),float(bars[2]),float(bars[3])},4};
            result["bBarColor"]={{bar_color[0],bar_color[1],bar_color[2],0},3};
        }
        return result;
    }
private:
    struct Shift {double hue,alpha;};
    std::string active;
    bool started=false,rising=true;
    double remainder=0,police=100,tint=0,shift_tick=0;
    std::uint32_t random=0x942ac619u;
    std::vector<Shift> shifts;
    std::array<std::array<float,3>,4> palette{};
    unsigned bar_count=0;
    std::array<double,4> bars{};
    std::array<float,3> bar_color{};
    void SetBars(const nlohmann::json& p)
    {
        const auto count=unsigned(Number(p,"barAmount",2,1,4));
        if(count==bar_count)return;
        bar_count=count;
        for(unsigned i=0;i<4;++i)bars[i]=400.0*i/count;
    }
    static std::array<float,3> Color(const std::string& text)
    {
        unsigned rgb=0;
        try {if(text.size()==7&&text[0]=='#')rgb=unsigned(std::stoul(text.substr(1),nullptr,16));} catch(...){}
        return {float((rgb>>16)&255)/255,float((rgb>>8)&255)/255,float(rgb&255)/255};
    }
    double Random()
    {
        random^=random<<13;random^=random>>17;random^=random<<5;
        return double(random&0xffffffu)/16777216.0;
    }
    static double Number(const nlohmann::json& p,const char* key,double fallback,double minimum,double maximum)
    {const double v=p.value(key,fallback);return std::isfinite(v)?std::clamp(v,minimum,maximum):fallback;}
    void LoadPalette(const nlohmann::json& p)
    {
        static const char* defaults[]={"#ff0000","#00ff00","#0000ff","#ffff00"};
        for(unsigned i=0;i<4;++i)
        {
            const auto text=p.value("color"+std::to_string(i+1),std::string(defaults[i]));
            palette[i]=Color(text);
        }
    }
};
}
