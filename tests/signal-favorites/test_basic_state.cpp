// SPDX-License-Identifier: GPL-2.0-or-later
#include "BasicEffectState.h"
#include <iostream>
#include <limits>
#include <stdexcept>
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error(#x);}while(0)
bool near(double a,double b){return std::abs(a-b)<1e-5;}
float Scalar(const ShaderUniformMap& u,const char* n){return u.at(n).values[0];}
int main()
{
    try
    {
        native_basic::State s;
        CHECK(s.Update("Other",{},1).empty());CHECK(native_basic::State::Declarations("Other").empty());
        auto u=s.Update("PoliceLights",{{"speed",50}},0);CHECK(Scalar(u,"bPoliceTiming")==100);
        u=s.Update("PoliceLights",{{"speed",50}},1.0/60);CHECK(Scalar(u,"bPoliceTiming")==95);
        u=s.Update("PoliceLights",{{"speed",4}},.2);CHECK(Scalar(u,"bPoliceTiming")==95);
        u=s.Update("PoliceLights",{{"speed",100}},1.0/60);CHECK(Scalar(u,"bPoliceTiming")==85);
        u=s.Update("PoliceLights",{{"speed",5}},1.0/60);CHECK(Scalar(u,"bPoliceTiming")==84);
        u=s.Update("PoliceLights",{{"speed",100}},std::numeric_limits<double>::quiet_NaN());CHECK(Scalar(u,"bPoliceTiming")==84);
        native_basic::State a,b;a.Update("PoliceLights",{{"speed",70}},0);b.Update("PoliceLights",{{"speed",70}},0);
        auto together=a.Update("PoliceLights",{{"speed",70}},.2);
        ShaderUniformMap split;for(int i=0;i<12;++i)split=b.Update("PoliceLights",{{"speed",70}},1.0/60);
        CHECK(Scalar(together,"bPoliceTiming")==Scalar(split,"bPoliceTiming"));
        s.Reset();u=s.Update("ColorShift",{{"shiftSpeed",50}},0);CHECK(Scalar(u,"bShiftCount")==1);CHECK(u.at("bShift[0]").values[1]==0);
        u=s.Update("ColorShift",{{"shiftSpeed",50}},.1);CHECK(near(u.at("bShift[0]").values[1],.3));
        const float hue=u.at("bShift[0]").values[0];u=s.Update("ColorShift",{{"shiftSpeed",0}},.1);
        CHECK(near(u.at("bShift[0]").values[1],.3));CHECK(u.at("bShift[0]").values[0]==hue);
        for(int i=0;i<30;++i)u=s.Update("ColorShift",{{"shiftSpeed",100}},.25);
        CHECK(Scalar(u,"bShiftCount")==10);CHECK(u.size()==11);
        for(int i=0;i<10;++i){const auto p=u.at("bShift["+std::to_string(i)+"]").values;CHECK(p[0]>=0&&p[0]<1&&p[1]>=0&&p[1]<=1);}
        s.Reset();u=s.Update("ColorShift",{{"shiftSpeed",50}},0);CHECK(Scalar(u,"bShiftCount")==1);CHECK(u.at("bShift[0]").values[0]==hue);
        s.Reset();u=s.Update("QuadColorBreath",{},0);CHECK(u.at("bQuadrant[0]").values[0]==1);CHECK(Scalar(u,"bBreathTint")==0);
        for(int i=0;i<14;++i)u=s.Update("QuadColorBreath",{{"rotate",true},{"speed",1}},.25);
        CHECK(u.at("bQuadrant[0]").values[0]==1&&u.at("bQuadrant[0]").values[1]==1);
        auto hold=s.Update("QuadColorBreath",{{"rotate",true},{"color1","#ffffff"}},0);
        CHECK(hold.at("bQuadrant[0]").values[2]==0);
        u=s.Update("QuadColorBreath",{{"rotate",false},{"color1","#ffffff"}},0);CHECK(u.at("bQuadrant[0]").values[2]==1);
        s.Reset();u=s.Update("CrookedWaves",{{"barAmount",2},{"speed",3}},0);
        CHECK(u.at("bBarPositions").values[0]==0&&u.at("bBarPositions").values[1]==200);
        u=s.Update("CrookedWaves",{{"barAmount",2},{"speed",3}},.1);CHECK(u.at("bBarPositions").values[0]==18);
        u=s.Update("CrookedWaves",{{"barAmount",4},{"speed",3}},0);CHECK(u.at("bBarPositions").values[1]==100);
        u=s.Update("CrookedWaves",{{"barAmount",4},{"color","#00ff00"}},0);CHECK(u.at("bBarColor").values[1]==1);
        for(unsigned i=0;i<10;++i)u=s.Update("CrookedWaves",{{"barAmount",4},{"speed",10},{"barWidth",50},{"randomColors",true}},.25);
        for(float x:u.at("bBarPositions").values)CHECK(x>=-50&&x<350);
        std::cout<<"PASS "<<checks<<" basic state assertions: fixed-step histories, speed edits, freeze, palette, reset, caps\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
