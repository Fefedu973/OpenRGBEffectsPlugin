// SPDX-License-Identifier: GPL-2.0-or-later
#include <cstdio>
#define LOG_WARNING(...) ((void)0)
#include "Audio/AudioManager.h"
#include "Audio/AudioManagerWin.h"
#include <iostream>
#include <limits>
#include <stdexcept>
static unsigned checks=0;
static void Check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
int main()
{
    try
    {
        using namespace audio_pcm;
        const Format stereo{Encoding::Float32,2,8};
        Window a,b;
        float samples[]={0.8f,0.2f,-0.2f,-0.8f};
        Check(a.Append(reinterpret_cast<const uint8_t*>(samples),2,stereo,false),"stereo float accepted");
        auto s=a.Snapshot();
        Check(std::abs(s[510]-0.5f)<1e-6f&&std::abs(s[511]+0.5f)<1e-6f,"stereo downmix uses frames and channel count");
        Check(std::all_of(s.begin(),s.begin()+510,[](float n){return n==0;}),"unwritten history zero initialized");
        auto untouched=b.Snapshot();
        Check(std::all_of(untouched.begin(),untouched.end(),[](float n){return n==0;}),"capture histories are independent");
        Check(a.Append(nullptr,512,stereo,true),"silent WASAPI packet accepts null data");
        s=a.Snapshot();Check(std::all_of(s.begin(),s.end(),[](float n){return n==0;}),"silent packet clears complete window");
        Check(!a.Append(nullptr,1,stereo,false),"non-silent null buffer rejected");
        Check(!Valid({Encoding::Signed16,6,4}),"undersized channel stride rejected");
        int16_t pcm16[]={32767,-32768};
        Check(a.Append(reinterpret_cast<uint8_t*>(pcm16),2,{Encoding::Signed16,1,2},false),"PCM16 mono accepted");
        s=a.Snapshot();Check(s[510]>0.999f&&s[511]==-1.f,"PCM16 signed extremes");
        uint8_t pcm24[]={0xff,0xff,0x7f,0,0,0x80};
        Check(Sample(pcm24,Encoding::Signed24)>0.999f&&Sample(pcm24+3,Encoding::Signed24)==-1.f,"PCM24 sign extension");
        uint8_t pcm8[]={0,128,255};
        Check(Sample(pcm8,Encoding::Unsigned8)==-1.f&&Sample(pcm8+1,Encoding::Unsigned8)==0.f,"PCM8 unsigned center");
        int32_t pcm32=INT32_MIN;
        Check(Sample(reinterpret_cast<uint8_t*>(&pcm32),Encoding::Signed32)==-1.f,"PCM32 extreme");
        double f64=0.25;
        Check(Sample(reinterpret_cast<uint8_t*>(&f64),Encoding::Float64)==0.25f,"float64 accepted");
        float invalid=std::numeric_limits<float>::quiet_NaN();
        Check(Sample(reinterpret_cast<uint8_t*>(&invalid),Encoding::Float32)==0.f,"NaN cannot poison FFT history");
        float six[]={0.6f,0.6f,0.6f,0.6f,0.6f,0.6f};
        b.Append(reinterpret_cast<uint8_t*>(six),1,{Encoding::Float32,6,24},false);
        Check(std::abs(b.Snapshot()[511]-0.6f)<1e-6f,"six-channel frame consumed with actual channel count");
        WAVEFORMATEX wave{};wave.wFormatTag=WAVE_FORMAT_IEEE_FLOAT;wave.wBitsPerSample=32;
        wave.nChannels=2;wave.nBlockAlign=8;wave.nSamplesPerSec=48000;Format fmt{};
        Check(AudioFormat(&wave,fmt)&&fmt.channels==2,"real WAVEFORMATEX IEEEfloat parser");
        wave.wFormatTag=0x1234;Check(!AudioFormat(&wave,fmt),"unknown compressed format rejected");
        WAVEFORMATEXTENSIBLE ext{};ext.Format=wave;ext.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;
        ext.Format.cbSize=sizeof(ext)-sizeof(WAVEFORMATEX);ext.SubFormat=KSDATAFORMAT_SUBTYPE_PCM;
        Check(AudioFormat(&ext.Format,fmt)&&fmt.encoding==Encoding::Signed32,"extensible PCM subtype recognized");
        ext.Format.cbSize=0;Check(!AudioFormat(&ext.Format,fmt),"truncated extensible format rejected");
        auto session=std::make_shared<AudioSession>();
        session->worker=std::thread([session]{session->Wait(10000);});
        auto before=std::chrono::steady_clock::now();session->Stop();
        Check(std::chrono::steady_clock::now()-before<std::chrono::milliseconds(250),"stop interrupts pending wait without ten-second join");
        // Enumeration only. No RegisterClient(valid), Activate, Start or capture.
        auto* manager=AudioManager::get();auto devices=manager->GetAudioDevices();
        Check(!devices.empty()&&std::string(devices.back())=="System default output (Loopback)","stable default-output choice appended after physical endpoint list");
        std::array<float,512> output;output.fill(42.f);manager->Capture(-1,output.data());
        Check(std::all_of(output.begin(),output.end(),[](float n){return n==0;}),"missing capture always returns 512 zero samples");
        manager->RegisterClient(-1,reinterpret_cast<void*>(1));manager->UnRegisterClient(-1,reinterpret_cast<void*>(1));
        std::cout<<"PASS "<<checks<<" audio format/lifecycle assertions; enumerated "<<devices.size()<<" endpoints, no stream opened\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
