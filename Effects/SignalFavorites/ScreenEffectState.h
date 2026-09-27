// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ShaderPass.h"
#include <QImage>
#include <QColor>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Numeric state only. The OpenGL shaders draw every pixel. One owner calls
// Update per effect-render step; capture reduction is cached by sequence/size.
namespace native_screen {
struct Result {
    ShaderUniformMap uniforms;
    std::shared_ptr<const std::vector<float>> numericRGBA;
    unsigned width=0,height=0;
    uint64_t sequence=0;
};
class State {
    struct Hsl {float h=0,s=0,l=0;};
    struct Wave {double radius;uint64_t count;};
public:
    void Reset(){
        // Do not construct a half-megabyte temporary on the render worker's
        // default Windows stack. The large histories already belong to State.
        active.clear();cached=hsl_initialized=hd_initialized=overflow=false;
        source_size={};source_sequence=result_sequence=reductions=0;
        raw.fill({});hsl_history.fill({});std::fill(hd_raw.begin(),hd_raw.end(),0);std::fill(hd_history.begin(),hd_history.end(),0);
        radii.fill(320.0/56);growing.fill(false);waves.clear();
        gain=1;dominant=-1;cycle=0;wave_counter=1;
    }
    static std::string Declarations(const std::string&){return "uniform float bScreenDominantHue;\nuniform float bScreenHD;\nuniform float bScreenMaxRadius;\n";}
    Result Update(const std::string&id,const json& parameters,double /*dt*/,const QImage& source,uint64_t sequence,unsigned helperTaps)
    {
        if(id!="AverageColor"&&id!="ScreenAmbience"&&id!="LSDAmbience")return {};
        const json empty=json::object();const auto& p=parameters.is_object()?parameters:empty;
        if(active!=id){Reset();active=id;}
        if(source.isNull())return {};
        if(!cached||sequence!=source_sequence||source.size()!=source_size){Reduce(source);source_sequence=sequence;source_size=source.size();cached=true;++reductions;}
        Result result;auto data=std::make_shared<std::vector<float>>();
        if(id=="AverageColor") {
            if(p.value("tapOn",false))gain*=std::pow(.95,double(std::min(helperTaps,64u)));
            double h=0,s=0,l=0;for(const auto& zone:raw){h+=zone.h;s+=zone.s;l+=zone.l;}
            *data={float(h/(560*360)),float(s/56000),float(l/56000),float(std::clamp(gain,0.0,1.0))};
            result.width=result.height=1;
            gain=gain<1?gain+.025:1;
        } else if(id=="ScreenAmbience") {
            const std::string mode=p.value("picture_mode",std::string("Standard"));
            const bool hd=mode=="HD";const double smoothing=Number(p,"motion_smoothing",50,0,100);
            const double alpha=smoothing<=0?1:std::max(.05,1-smoothing/100);
            if(hd) {
                if(!hd_initialized||alpha>=1){hd_history=hd_raw;hd_initialized=true;}
                else for(std::size_t i=0;i<hd_history.size();++i)hd_history[i]=float(alpha*hd_raw[i]+(1-alpha)*hd_history[i]);
                data->resize(hd_history.size());for(std::size_t i=0;i<data->size();++i)(*data)[i]=float(ClampedByte(hd_history[i])/255.0);
                result.width=160;result.height=100;
            } else {
                if(!hsl_initialized){hsl_history=raw;hsl_initialized=true;}
                if(mode=="Dominant") {
                    unsigned selected=0;
                    // Preserve the observed last-eligible-cell behavior, not a
                    // newly invented maximum-saturation selector.
                    for(unsigned i=0;i<560;++i)if(raw[i].l>30&&raw[i].s>0)selected=i;
                    if(dominant<0)dominant=raw[selected].h;
                    else if(raw[selected].h>dominant)dominant+=1;
                    else if(raw[selected].h<dominant)dominant-=1;
                }
                data->resize(560*4);
                for(unsigned i=0;i<560;++i) {
                    auto& v=hsl_history[i];const auto target=raw[i];
                    if(alpha>=1)v=target;
                    else {double delta=target.h-v.h;if(delta>180)delta-=360;else if(delta< -180)delta+=360;
                        double h=v.h+alpha*delta;if(h<0)h+=360;else if(h>=360)h-=360;
                        v={float(h),float(alpha*target.s+(1-alpha)*v.s),float(alpha*target.l+(1-alpha)*v.l)};}
                    (*data)[4*i]=v.h/360;(*data)[4*i+1]=v.s/100;(*data)[4*i+2]=v.l/100;(*data)[4*i+3]=1;
                }
                result.width=28;result.height=20;
            }
            result.uniforms["bScreenHD"].values[0]=hd?1.f:0.f;
            result.uniforms["bScreenDominantHue"].values[0]=float(std::max(0.0,dominant)/360);
        } else {
            UpdateDots(p,*data,result.uniforms);result.width=28;result.height=20;
        }
        result.numericRGBA=std::move(data);result.sequence=++result_sequence;return result;
    }
    uint64_t ReductionCount()const{return reductions;}
    bool Overflowed()const{return overflow;}
private:
    static double Number(const json&p,const char*key,double fallback,double low,double high){auto it=p.find(key);if(it==p.end()||!it->is_number())return fallback;const double v=it->get<double>();return std::isfinite(v)?std::clamp(v,low,high):fallback;}
    static Hsl ToHsl(double r,double g,double b){const double hi=std::max({r,g,b}),lo=std::min({r,g,b}),d=hi-lo,l=(hi+lo)/2;double h=0,s=0;if(d>0){s=d/(1-std::abs(2*l-1));if(hi==r)h=std::fmod((g-b)/d,6.0);else if(hi==g)h=(b-r)/d+2;else h=(r-g)/d+4;h*=60;if(h<0)h+=360;}return {float(std::floor(h+.5)),float(std::floor(s*100+.5)),float(std::floor(l*100+.5))};}
    static double ClampedByte(double v){v=std::clamp(v,0.0,255.0);const double lower=std::floor(v),fraction=v-lower;if(fraction<.5)return lower;if(fraction>.5)return lower+1;return std::fmod(lower,2)==0?lower:lower+1;}
    void Reduce(const QImage& image){
        // Engine.zone's proprietary screen reduction is unavailable. Qt smooth
        // full-rectangle reduction is explicit; no implicit crop or aspect fit.
        const auto grid=image.scaled(28,20,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        for(unsigned y=0;y<20;++y)for(unsigned x=0;x<28;++x){const auto c=grid.pixelColor(x,y);raw[y*28+x]=ToHsl(c.redF(),c.greenF(),c.blueF());}
        const auto hd=image.scaled(160,100,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        for(unsigned y=0;y<100;++y)for(unsigned x=0;x<160;++x){const auto c=hd.pixelColor(x,y);const auto i=4*(y*160+x);hd_raw[i]=float(c.red());hd_raw[i+1]=float(c.green());hd_raw[i+2]=float(c.blue());hd_raw[i+3]=255;}
    }
    static void Add(std::vector<Wave>& target,double radius,uint64_t count){if(!count)return;if(!target.empty()&&target.back().radius==radius)target.back().count+=count;else target.push_back({radius,count});}
    void AdvanceWaves(double step){
        // Run-length encoding keeps zero-speed births bounded without losing
        // multiplicity, forward-splice skip behavior, or the stationary tail.
        if(waves.empty())return;std::vector<Wave> next;next.reserve(waves.size()+1);bool skipped=false;
        for(std::size_t i=0;i<waves.size();++i){auto w=waves[i];const bool tail=i+1==waves.size();if(tail)--w.count;
            if(skipped&&w.count){Add(next,w.radius,1);--w.count;skipped=false;}
            if(w.count){if(w.radius<250)Add(next,w.radius+step,w.count);else {Add(next,w.radius,w.count/2);skipped=(w.count%2)!=0;}}
            if(tail)Add(next,w.radius,1);
        }
        if(next.size()>8192){next.erase(next.begin(),next.begin()+(next.size()-8192));overflow=true;}
        waves=std::move(next);
    }
    void UpdateDots(const json&p,std::vector<float>& data,ShaderUniformMap& uniforms){
        const double frequency=Number(p,"waveFreq",15,0,100),speed=Number(p,"effectSpeed",25,0,100),minimum=Number(p,"dotMin",25,0,100)/4;
        const double decay=.75+.249*(1-Number(p,"dotFade",5,0,100)/100);
        const std::string mode=p.value("colorMode",std::string("Color Cycle"));
        if(wave_counter==0)Add(waves,1,1);
        if(frequency!=0)wave_counter=wave_counter<100-frequency?wave_counter+1:0;
        if(mode=="Color Cycle"||mode=="Rainbow Gradient")cycle=cycle<360?cycle+Number(p,"cycleSpeed",50,0,100)/50:0;else cycle=0;
        const QColor custom(QString::fromStdString(p.value("color1",std::string("#0000ff"))));const auto custom_hsl=ToHsl(custom.redF(),custom.greenF(),custom.blueF());
        data.resize(560*4);double maximum=0;
        for(unsigned i=0;i<560;++i){
            const double x=(i%28)*(320.0/28)+5,y=(i/28)*10+5,dist=std::hypot(x-160,y-100);
            const double hue=mode=="Custom"?custom_hsl.h:raw[i].h+cycle+(mode=="Rainbow Gradient"?i:0);
            data[4*i]=float((hue+cycle)/360);data[4*i+1]=std::max(.4f,raw[i].s/100);data[4*i+2]=std::max(.01f,raw[i].l/100);data[4*i+3]=float(radii[i]);maximum=std::max(maximum,radii[i]);
            for(const auto& w:waves)if(std::abs(w.radius-dist)<5){growing[i]=true;break;}
            if(growing[i]){if(radii[i]<minimum*3)radii[i]+=(minimum*3.5-radii[i])/10;else growing[i]=false;}
            else if(radii[i]>minimum)radii[i]*=decay;else radii[i]=minimum;
        }
        AdvanceWaves(speed/10);uniforms["bScreenMaxRadius"].values[0]=float(maximum);
    }
    std::string active;bool cached=false,hsl_initialized=false,hd_initialized=false,overflow=false;
    QSize source_size;uint64_t source_sequence=0,result_sequence=0,reductions=0;
    std::array<Hsl,560> raw{},hsl_history{};
    std::vector<float> hd_raw=std::vector<float>(16000*4),hd_history=std::vector<float>(16000*4);
    std::array<double,560> radii=[] {std::array<double,560> a{};a.fill(320.0/56);return a;}();
    std::array<bool,560> growing{};std::vector<Wave>waves;double gain=1,dominant=-1,cycle=0;unsigned wave_counter=1;
};
}
