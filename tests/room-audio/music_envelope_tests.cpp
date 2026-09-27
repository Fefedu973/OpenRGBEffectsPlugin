// SPDX-License-Identifier: GPL-2.0-or-later
#include "MusicEnvelope.h"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
static unsigned checks=0;
static void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
static void Bass(std::array<float,256>& values,float value)
{values.fill(0.f);for(int bin=1;bin<=3;++bin)for(int repeat=0;repeat<4;++repeat)values[bin*4+repeat]=value;}
int main()
{
    try
    {
        MusicEnvelope state;std::array<float,256> audio{};
        auto idle=state.Update(audio.data(),0.0);
        Check(idle[0]==0&&idle[1]==0&&idle[3]==0,"silence has no envelope or onset");
        for(int i=1;i<60;++i)Check(state.Update(audio.data(),i/60.0)[2]==idle[2],"time alone cannot change hue");
        Bass(audio,0.4f);auto attack=state.Update(audio.data(),1.0);
        Check(attack[0]>0&&attack[1]>0.5f&&attack[3]==1,"bass transient creates actual onset");
        Check(attack[2]!=idle[2],"onset changes hue");
        auto sustained=attack;
        for(int i=1;i<=120;++i)sustained=state.Update(audio.data(),1.0+i/60.0);
        Check(sustained[2]==attack[2],"sustained tone does not retrigger color");
        Check(sustained[3]<0.0001f,"onset pulse decays without a new transient");
        audio.fill(0.f);auto released=state.Update(audio.data(),3.1);
        Check(released[0]<sustained[0]&&released[1]<sustained[1],"silence releases envelopes");
        for(int i=1;i<60;++i)state.Update(audio.data(),3.1+i/60.0);
        Bass(audio,0.7f);auto second=state.Update(audio.data(),4.2);
        Check(second[2]!=attack[2]&&second[3]==1,"separated bass attack changes hue again");
        audio.fill(0.f);state.Update(audio.data(),4.22);Bass(audio,0.8f);
        Check(state.Update(audio.data(),4.25)[2]==second[2],"refractory interval prevents rapid retrigger");
        auto missing=state.Update(nullptr,4.26);
        Check(missing[0]==0&&missing[1]==0&&missing[3]==0,"disconnected audio immediately clears energy");
        audio.fill(0.f);for(int i=96;i<256;++i)audio[i]=0.8f;
        MusicEnvelope treble;auto high=treble.Update(audio.data(),0);
        Check(high[0]>0.5f&&high[1]==0&&high[3]==0,"treble drives broadband volume but not bass onset");
        audio.fill(std::numeric_limits<float>::quiet_NaN());auto invalid=treble.Update(audio.data(),0.1);
        for(float value:invalid)Check(std::isfinite(value),"non-finite samples cannot poison uniforms");
        MusicEnvelope fps30,fps60;Bass(audio,0.4f);fps30.Update(audio.data(),0);fps60.Update(audio.data(),0);audio.fill(0.f);
        std::array<float,4> a{},b{};
        for(int i=1;i<=15;++i)a=fps30.Update(audio.data(),i/30.0);
        for(int i=1;i<=30;++i)b=fps60.Update(audio.data(),i/60.0);
        Check(std::abs(a[0]-b[0])<0.00001f&&std::abs(a[1]-b[1])<0.00001f,"release uses seconds rather than frame count");
        Bass(audio,0.5f);state.Update(audio.data(),5.0);audio.fill(0.f);auto pause=state.Update(audio.data(),10.0);
        Check(pause[0]==0&&pause[1]==0&&pause[3]==0,"long gaps discard stale state");
        std::cout<<"PASS "<<checks<<" music envelope checks\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
