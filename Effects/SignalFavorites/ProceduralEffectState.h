// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ShaderPass.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

// Numeric state only: no per-pixel CPU renderer or browser implementation.
// Discrete reference animations advance at 60 Hz, with at most 15 catch-up ticks.
namespace native_procedural
{
class State
{
public:
    void Reset(){*this=State{};}
    // Each individual shader declares its own bounded numeric uniforms.
    static std::string Declarations(const std::string&){return {};}
    ShaderUniformMap Update(const std::string& id,const nlohmann::json& p,double elapsed)
    {
        if(id!="Visor"&&id!="CustomWave"&&id!="Pinwheel"&&id!="Spin"&&id!="Plasma")return {};
        if(active!=id){Reset();active=id;}
        const double dt=std::isfinite(elapsed)?std::clamp(elapsed,0.0,.25):0;
        clock+=dt;remainder+=dt;
        if(!started){started=true;color=Color(p,"color",0xff0066);Tick(p);}
        const unsigned count=unsigned(std::floor((remainder+1e-10)*60));
        remainder=std::max(0.0,remainder-count/60.0);
        for(unsigned i=0;i<count;++i)Tick(p);
        ShaderUniformMap result;
        if(id=="Spin")result["prState"]={{float(clock*1000),0,0,0},4};
        else result["prState"]={display,4};
        if(id=="Visor")result["prColor"]={{display_color[0],display_color[1],display_color[2],0},3};
        return result;
    }
private:
    std::string active;
    bool started=false,negative_v=false,negative_h=false;
    double remainder=0,clock=0,vertical=320,horizontal=200,offset=0,degree=1,x=0,y=0,noise_time=0,gradient_time=0;
    std::array<float,4> display{};
    std::array<float,3> color{},display_color{};
    std::uint32_t random=0x55bd1739u;
    static double N(const nlohmann::json& p,const char* k,double def,double lo,double hi)
    {auto i=p.find(k);if(i==p.end()||!i->is_number())return def;double n=i->get<double>();return std::isfinite(n)?std::clamp(n,lo,hi):def;}
    static bool B(const nlohmann::json& p,const char* k,bool def)
    {auto i=p.find(k);return i!=p.end()&&i->is_boolean()?i->get<bool>():def;}
    static std::string S(const nlohmann::json& p,const char* k,const char* def)
    {auto i=p.find(k);return i!=p.end()&&i->is_string()?i->get<std::string>():def;}
    static std::array<float,3> Color(const nlohmann::json& p,const char* key,unsigned fallback)
    {
        unsigned rgb=fallback;auto it=p.find(key);
        if(it!=p.end()&&it->is_string())try{const std::string s=it->get<std::string>();if(s.size()==7&&s[0]=='#')rgb=unsigned(std::stoul(s.substr(1),nullptr,16));}catch(...){}
        return {float((rgb>>16)&255)/255,float((rgb>>8)&255)/255,float(rgb&255)/255};
    }
    std::array<float,3> RandomColor()
    {random^=random<<13;random^=random>>17;random^=random<<5;return {float(random&255)/255,float((random>>8)&255)/255,float((random>>16)&255)/255};}
    void Tick(const nlohmann::json& p)
    {
        if(active=="Visor")
        {
            const bool v=B(p,"vertical",true),random_colors=B(p,"randomColors",false);
            const double speed=N(p,"speed",30,0,100)/10,width=N(p,"barWidth",20,1,50);
            display={float(v?vertical:horizontal),0,0,0};display_color=color;
            if(v)
            {
                vertical+=(negative_v?-speed:speed);
                if(vertical>=320-width){negative_v=true;color=random_colors?RandomColor():Color(p,"color",0xff0066);}
                else if(vertical<=0){negative_v=false;color=random_colors?RandomColor():Color(p,"color",0xff0066);}
            }
            else
            {
                horizontal+=(negative_h?-speed:speed);
                if(horizontal>200-width){negative_h=true;color=random_colors?RandomColor():Color(p,"color",0xff0066);}
                else if(horizontal<0){negative_h=false;color=random_colors?RandomColor():Color(p,"color",0xff0066);}
            }
            if(!random_colors)color=Color(p,"color",0xff0066);
        }
        else if(active=="CustomWave")
        {
            offset+=N(p,"speed",50,0,100)/20;
            display={float(offset),0,0,0};
            const unsigned colors=unsigned(std::floor(N(p,"nColors",2,2,4)+.5));
            const double limit=B(p,"bVertical",false)?(colors==3?150:200):(colors==4?200:300);
            if(offset>limit)offset=0;
        }
        else if(active=="Pinwheel")
        {
            const bool bounce=B(p,"bounce",false);
            display={float(degree),float(bounce?x:N(p,"num1",160,0,320)),float(bounce?y:N(p,"num2",100,0,200)),0};
            if(bounce)
            {
                const double s=N(p,"bounceSpeed",40,1,100)/10;
                if(S(p,"edgeToEdge","Clockwise")=="Clockwise")
                {
                    if(x<=320&&y<=0){x+=s;y=0;}
                    if(x>=320&&y<=200){y+=s;x=320;}
                    if(y>=200&&x<=321){x-=s;y=200;}
                    if(y<=201&&x<=0){y-=s;x=0;}
                }
                else
                {
                    if(x<=321&&y<=0){x-=s;y=0;}
                    if(x>=320&&y<=201){y-=s;x=320;}
                    if(y>=200&&x<=321){x+=s;y=200;}
                    if(y<=201&&x<=0){y+=s;x=0;}
                }
            }
            if(degree<360)degree+=(S(p,"direction","Clockwise")=="Clockwise"?1:-1)*N(p,"speed",10,0,100)/20;
            else degree=0;
            // The source lets negative angles grow indefinitely. Equivalent
            // 360-degree wrapping preserves the geometry without float loss.
            if(degree<-360)degree+=360;
        }
        else if(active=="Plasma")
        {
            const double previous=noise_time;
            noise_time+=.0002*N(p,"speed",20,0,100);
            gradient_time=gradient_time<385?gradient_time+5:0;
            display={float(noise_time),float(gradient_time),float(previous),0};
        }
    }
};
}
