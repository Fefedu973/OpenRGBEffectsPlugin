// SPDX-License-Identifier: GPL-2.0-or-later
// Synthetic PCM through the real Windows publication path. No stream/device opened.
#define LOG_WARNING(...) ((void)0)
#include "Audio/AudioManager.h"
#include "Audio/AudioManagerWin.h"
#include <iostream>
#include <stdexcept>
#include <thread>

static unsigned checks=0;
static void Check(bool v,const char* message) { ++checks; if(!v)throw std::runtime_error(message); }
using room_audio::PcmWindowBuffer;
using room_audio::PcmWindowStatus;
constexpr double Pi=3.14159265358979323846;

static std::vector<float> Tone(unsigned rate,double seconds,double frequency=997)
{
    std::vector<float> out(std::size_t(rate*seconds));
    for(std::size_t i=0;i<out.size();++i)out[i]=float(0.6*std::sin(2*Pi*frequency*i/rate));
    return out;
}
static void Feed(PcmWindowBuffer& buffer,const std::vector<float>& pcm,unsigned rate,
                 double start=100,bool fragmented=true,unsigned channels=2)
{
    std::size_t at=0,packet=0;
    const std::size_t sizes[]={1,127,960,2048,4096,73,511};
    while(at<pcm.size())
    {
        const std::size_t n=std::min(pcm.size()-at,fragmented?sizes[packet++%7]:std::size_t(rate));
        Check(buffer.Append(pcm.data()+at,n,rate,channels,start+double(at)/rate,false,true,at),"append finite source packet");
        at+=n;
    }
}
static void RatesAndBounds()
{
    for(unsigned rate:{8000u,22050u,44100u,48000u,96000u,192000u})
    {
        PcmWindowBuffer whole,fragments;
        const auto pcm=Tone(rate,5.1);
        Feed(whole,pcm,rate,100,false); Feed(fragments,pcm,rate);
        const auto begin=std::chrono::steady_clock::now();
        auto a=whole.Read(240000,105.1),b=fragments.Read(240000,105.1);
        const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        Check(a.Ready()&&b.Ready(),"complete five-second windows available at all supported source rates");
        Check(a.mono.size()==240000 && a.sample_rate==48000 && a.source_rate==rate && a.source_channels==2,"fixed model shape and original metadata");
        Check(std::abs(a.source_end-a.source_begin-5.0)<1e-9,"model window exactly five seconds");
        Check(a.source_end<=a.captured_through+1e-9,"no future samples consumed");
        Check(a.epoch!=b.epoch,"independent sessions have distinct epochs");
        double maximum=0,error=0;
        for(std::size_t i=0;i<a.mono.size();++i)
        {
            maximum=std::max(maximum,double(std::abs(a.mono[i]-b.mono[i])));
            const double expected=0.6*std::sin(2*Pi*997*(a.source_begin-100+double(i)/48000));
            error+=std::pow(a.mono[i]-expected,2);
        }
        Check(maximum<1e-7,"packet fragmentation cannot change samples");
        Check(std::sqrt(error/a.mono.size())<0.002,"band-limited tone phase/amplitude maintained");
        if(rate==48000)
            Check(std::equal(a.mono.begin(),a.mono.end(),pcm.end()-240000),"native48k path bit exact");
        auto one=whole.Read(1,105.1);
        Check(one.Ready() && one.mono.size()==1 && std::isfinite(one.mono[0]),"single-sample bound");
        Check(whole.Read(240001,105.1).status==PcmWindowStatus::InvalidRequest,"oversized model rejected");
        Check(whole.Read(0,105.1).status==PcmWindowStatus::InvalidRequest,"empty model rejected");
        Check(whole.Read(240000,106).status==PcmWindowStatus::Stale,"old window cannot feed live inference");
        Check(whole.Read(240000,1).status==PcmWindowStatus::Stale,"wrong clock domain rejected");
        std::cout<<"rate="<<rate<<" two_5s_reads_ms="<<elapsed<<" rms="<<std::sqrt(error/a.mono.size())<<"\n";
    }
    PcmWindowBuffer antialias;
    Feed(antialias,Tone(192000,0.2,30000),192000);
    const auto down=antialias.Read(4800,100.2);
    double sum=0;for(float v:down.mono)sum+=v*v;
    Check(down.Ready() && std::sqrt(sum/down.mono.size())<0.025,"30kHz source does not alias audibly into 48k target");
}
static void Invalidations()
{
    PcmWindowBuffer buffer;
    Check(buffer.Read(4800,100).status==PcmWindowStatus::Unavailable,"inactive is unavailable");
    const auto pcm=Tone(48000,0.2);
    Feed(buffer,pcm,48000);
    auto previous=buffer.Read(4800,100.2);
    Check(previous.Ready(),"baseline ready");
    buffer.Invalidate();
    Check(buffer.Read(4800,100.2).status==PcmWindowStatus::Unavailable,"missing packet invalidates immediately");
    Check(buffer.Append(pcm.data(),100,48000,2,101,false,true,12345),"reconnect begins warmup");
    Check(buffer.Read(4800,101.002).status==PcmWindowStatus::Warming,"no invented zero padding after gap");
    Check(buffer.Read(1,101.002).epoch!=previous.epoch,"reconnect epoch changed");
    Check(buffer.Append(pcm.data(),100,48000,2,101+100.0/48000,false,true,99999),"unexpected device index starts another epoch");
    Check(buffer.Read(101,101.005).status==PcmWindowStatus::Warming,"device-index discontinuity drops old history");
    auto epoch=buffer.Read(1,101.005).epoch;
    Check(buffer.Append(pcm.data(),100,44100,2,101.01,false,true,100099),"rate change starts fresh");
    Check(buffer.Read(1,101.013).epoch!=epoch,"rate change epoch");
    Check(buffer.Append(nullptr,0,44100,2,101.02,true),"zero-count discontinuity still resets");
    Check(buffer.Read(1,101.02).status==PcmWindowStatus::Unavailable,"zero-count reset is observable");
    float bad=std::numeric_limits<float>::quiet_NaN();
    Check(!buffer.Append(&bad,1,48000,1,102),"nonfinite PCM rejected");
    Check(!buffer.Append(pcm.data(),1,384000,1,102),"unsupported high rate does not become falsely ready");
    Check(buffer.Read(1,102).status==PcmWindowStatus::UnsupportedRate,"unsupported rate distinguished");
    Check(!buffer.Append(nullptr,1,48000,1,102),"null data rejected");
    Check(!buffer.Append(pcm.data(),pcm.size(),48000,1,102,false,true,UINT64_MAX-1),"device counter overflow rejected");
    Check(!buffer.Append(pcm.data(),1,48000,33,102),"channels bounded");
    buffer.Close();
    Check(!buffer.Append(pcm.data(),1,48000,1,103),"closed reader cannot revive capture");
    buffer.Invalidate();
    Check(buffer.Read(1,103).status==PcmWindowStatus::Closed,"close stays final despite invalidate");
    Check(previous.Ready()&&previous.mono.size()==4800,"owned sample snapshot survives close");
}
static void RealPublication()
{
    auto session=std::make_unique<AudioSession>();
    Check(!std::atomic_load(&session->pcm_window),"no history allocated for legacy-only clients");
    auto buffer=std::make_shared<PcmWindowBuffer>();
    std::atomic_store(&session->pcm_window,buffer);
    audio_pcm::Window window;
    auto pcm=Tone(48000,0.2);
    Check(session->PublishPacket(window,pcm.data(),pcm.size(),48000,100,false,8,true,0),"real capture publication accepts PCM");
    auto snapshot=buffer->Read(4800,100.2);
    Check(snapshot.Ready()&&snapshot.source_channels==8,"existing session exposes correct mono metadata");
    Check(std::equal(session->buffer.begin(),session->buffer.end(),pcm.end()-512),"legacy512 remains unchanged");
    Check(session->rhythm_snapshot.sequence>0,"existing rhythm analysis still receives PCM");
    session->PublishNoPacket();
    Check(buffer->Read(1,100.2).status==PcmWindowStatus::Unavailable,"100ms no packet clears model history");
    Check(session->PublishPacket(window,pcm.data(),pcm.size(),48000,101,true,2,true,10000,true),"timestamp-error packet still serves legacy");
    Check(buffer->Read(1,101.2).status==PcmWindowStatus::Unavailable,"estimated timestamp excluded from model window");
    Check(session->PublishPacket(window,pcm.data(),pcm.size(),48000,102,true,2,true,20000),"good timestamp restarts window");
    Check(buffer->Read(4800,102.2).Ready(),"good source resumes model data");
    session->Stop();
    Check(buffer->Read(1,102.2).status==PcmWindowStatus::Closed,"existing session Stop closes held reader");
}
static void ConcurrentReaders()
{
    auto buffer=std::make_shared<PcmWindowBuffer>();
    const std::vector<float> constant(480,0.25f);
    std::atomic<bool> done{false},bad{false};
    Check(buffer->Append(constant.data(),480,48000,1,100,false,true,0),"concurrent stream begins");
    const auto epoch=buffer->Read(48,100.01).epoch;
    std::atomic<double> now{100.01};
    std::thread writer([&]{
        for(unsigned i=1;i<1000;++i)
        {
            buffer->Append(constant.data(),constant.size(),48000,1,100+i*.01,false,true,i*480);
            now.store(100+(i+1)*.01);
            if(i%10==0)std::this_thread::yield();
        }
        done.store(true);
    });
    auto read=[&]{while(!done.load())
    {
        const auto snapshot=buffer->Read(48,now.load());
        if(snapshot.Ready())for(float f:snapshot.mono)if(f!=0.25f)bad.store(true);
    }};
    std::thread a(read),b(read);
    writer.join();a.join();b.join();
    Check(!bad.load(),"two concurrent readers never see torn samples");
    const auto last=buffer->Read(48,110);
    Check(last.Ready() && last.epoch==epoch,"reader contention cannot invent capture discontinuities");
    buffer->Invalidate();
    Check(buffer->Append(constant.data(),480,48000,1,200,false,true,0),"producer resumes after reader contention");
    Check(buffer->Read(48,200.01).Ready(),"contention recovery yields complete fresh window");
}
int main()
{
    try { RatesAndBounds();Invalidations();RealPublication();ConcurrentReaders();
          std::cout<<checks<<" PCM window assertions PASS (synthetic; no capture opened)\n";return 0; }
    catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
