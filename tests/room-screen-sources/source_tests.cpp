/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ScreenSource.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#if defined(_WIN32) && !defined(SCREEN_SOURCES_DISABLE_FRAME_SURFACE)
#include <FrameSurface/FrameSurface.h>
#endif
using namespace std::chrono_literals;
using namespace screen_source;
static unsigned assertions=0;
static void Check(bool good,const char* message){if(!good)throw std::runtime_error(message);++assertions;}
template<class F> static Snapshot Wait(const std::shared_ptr<Source>& source,F predicate,const char* error)
{
    const auto until=std::chrono::steady_clock::now()+3s;
    do {auto s=source->Read();if(predicate(s))return s;std::this_thread::sleep_for(2ms);}while(std::chrono::steady_clock::now()<until);
    throw std::runtime_error(error);
}
static Config Configuration(const std::string& suffix)
{
    Config c;c.channel="screen-test-"+suffix;c.poll_ms=5;c.ttl_ms=1000;return c;
}
static std::vector<uint8_t> Pixels(unsigned width,unsigned height,uint8_t value,unsigned padding=0)
{
    const auto stride=width*4+padding;std::vector<uint8_t> p(size_t(stride)*height,91);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)
    {auto* b=p.data()+size_t(y)*stride+x*4;b[0]=value;b[1]=value^0x55;b[2]=255-value;b[3]=255;}
    return p;
}
static void ConfigTests()
{
    Config c;Check(c.Valid()&&c.channel=="better-screen-capture","Default config/channel invalid");c.channel.clear();Check(!c.Valid(),"Empty channel accepted");c.channel="bad/channel";Check(!c.Valid(),"Channel path accepted");
    c.channel="test";Check(c.Valid(),"Valid config rejected");c.poll_ms=0;Check(!c.Valid(),"Unbounded poll accepted");
    c.poll_ms=16;c.ttl_ms=0;Check(!c.Valid(),"Zero TTL accepted");c.ttl_ms=60001;Check(!c.Valid(),"Unbounded TTL accepted");
    c.ttl_ms=1000;c.max_width=0;Check(!c.Valid(),"Zero bound accepted");c.max_width=16385;Check(!c.Valid(),"Excess dimension bound accepted");
    bool rejected=false;try{Source::Acquire(c);}catch(const std::invalid_argument&){rejected=true;}Check(rejected,"Acquire ignored invalid config");
}
#if defined(_WIN32) && !defined(SCREEN_SOURCES_DISABLE_FRAME_SURFACE)
static void Lifecycle()
{
    const auto c=Configuration(std::to_string(GetCurrentProcessId())+"-life");
    auto source=Source::Acquire(c),same=Source::Acquire(c);Check(source==same,"Identical config duplicated worker");
    Wait(source,[](auto s){return s.state==State::Unavailable;},"Absent producer not reported");
    Check(!source->Read().Usable(),"Unavailable source was usable");
    auto publisher=std::make_unique<room_surface::Publisher>(c.channel,800*600*4);
    Check(publisher->IsOpen(),"Publisher failed");auto p=Pixels(2,2,20,4);
    Check(publisher->PublishBGRA(p.data(),p.size(),2,2,12),"Padded publish failed");
    auto received=Wait(source,[](auto s){return s.Usable();},"Frame not received");
    Check(received.frame->image.width()==2 && received.frame->image.height()==2 && received.frame->image.bytesPerLine()==12,"Image dimensions/stride changed");
    Check(received.frame->image.pixelColor(1,1)==QColor(235,20^0x55,20,255),"BGRA channel order wrong");
    Check(received.frame->image.constBits()[8]==0 && received.frame->image.constBits()[20]==0,"Padding not cleared by wire publisher");
    const auto original=received.frame;QImage edited=original->image;edited.setPixelColor(0,0,Qt::red);
    Check(original->image.pixelColor(0,0)!=Qt::red,"Copy-on-write mutated shared frame");
    auto stationary=Wait(source,[](auto s){return s.state==State::Static;},"Static state absent");
    const auto revision=stationary.revision;std::this_thread::sleep_for(30ms);
    Check(source->Read().frame==original && source->Read().revision==revision,"Static frame was copied or republished");
    for(unsigned i=1;i<=60;++i){p=Pixels(800,600,uint8_t(i));Check(publisher->PublishBGRA(p.data(),p.size(),800,600,3200),"Burst publication failed");}
    received=Wait(source,[](auto s){return s.Usable()&&s.frame->sequence==61;},"Latest burst frame not selected");
    Check(received.frame->image.size()==QSize(800,600) && qBlue(received.frame->image.pixel(799,599))==60,"HQ last frame incorrect");
    Check(original->image.size()==QSize(2,2) && qBlue(original->image.pixel(0,0))==20,"Retained immutable frame changed after later publication");
    same.reset();Check(source->Read().Usable(),"One subscriber stopped remaining subscriber");
    const auto generation=received.frame->generation;
    publisher.reset();
    Wait(source,[](auto s){return s.state==State::Unavailable;},"Producer close was not detected");
    Check(!source->Read().frame,"Unavailable snapshot retained a renderable frame");
    publisher=std::make_unique<room_surface::Publisher>(c.channel,800*600*4);
    Check(publisher->IsOpen(),"Replacement publisher failed");p=Pixels(3,1,99);
    Check(publisher->PublishBGRA(p.data(),p.size(),3,1,12),"Replacement publication failed");
    received=Wait(source,[&](auto s){return s.Usable()&&s.frame->generation!=generation;},"Reconnection generation was ignored");
    Check(received.frame->sequence==1&&received.frame->image.width()==3,"Reconnection retained old sequence/dimensions");
    const auto stop=std::chrono::steady_clock::now();source.reset();
    Check(std::chrono::steady_clock::now()-stop<200ms,"Worker stop was not prompt");
    Check(qBlue(original->image.pixel(0,0))==20,"Frame lifetime ended with worker");
}
static void ExpiryAndContention()
{
    auto c=Configuration(std::to_string(GetCurrentProcessId())+"-ttl");c.ttl_ms=120;
    room_surface::Publisher publisher(c.channel,1024);auto source=Source::Acquire(c);auto p=Pixels(4,4,7);
    Check(publisher.PublishBGRA(p.data(),p.size(),4,4,16),"TTL publication failed");
    auto fresh=Wait(source,[](auto s){return s.Usable();},"TTL frame absent");
    std::atomic<bool> held{false},release{false};
    std::thread holder([&]{HANDLE m=OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,room_surface::detail::Name(c.channel,true).c_str());
        {room_surface::detail::Lock guard(m,100);held=guard.acquired;while(!release)std::this_thread::sleep_for(1ms);}if(m)CloseHandle(m);});
    try
    {
        for(unsigned i=0;i<100&&!held;++i)std::this_thread::sleep_for(1ms);Check(held,"Contention setup failed");
        auto busy=Wait(source,[](auto s){return s.state==State::Busy;},"Busy state absent");Check(busy.Usable(),"Brief contention dropped a fresh image");
        Wait(source,[](auto s){return s.state==State::Stale;},"Contention kept expired frame live");
        Check(!fresh.Usable() && !source->Read().Usable(),"Retained snapshot bypassed TTL");
        const auto stop=std::chrono::steady_clock::now();source.reset();
        Check(std::chrono::steady_clock::now()-stop<200ms,"Contended worker stop waited for owner");
    }
    catch(...){release=true;holder.join();throw;}
    release=true;holder.join();
    source=Source::Acquire(c);Wait(source,[](auto s){return s.state==State::Stale;},"New reader accepted expired frame");
    Check(publisher.PublishBGRA(p.data(),p.size(),4,4,16),"Heartbeat publication failed");
    Check(Wait(source,[](auto s){return s.Usable();},"New sequence did not recover stale frame").frame->sequence==2,"Heartbeat sequence mismatch");
}
static void InvalidAndRecover()
{
    auto c=Configuration(std::to_string(GetCurrentProcessId())+"-bounds");c.max_width=8;c.max_height=8;
    room_surface::Publisher publisher(c.channel,4096);auto source=Source::Acquire(c);auto p=Pixels(9,2,3);
    Check(publisher.PublishBGRA(p.data(),p.size(),9,2,36),"Oversize fixture failed");
    Wait(source,[](auto s){return s.state==State::Invalid;},"Consumer dimension cap ignored");
    Check(!source->Read().Usable()&&!source->Read().frame,"Invalid dimensions became usable");
    std::this_thread::sleep_for(30ms);Check(source->Read().state==State::Invalid,"Rejected unchanged frame became Static");
    p=Pixels(4,4,8);Check(publisher.PublishBGRA(p.data(),p.size(),4,4,16),"Valid replacement failed");
    Wait(source,[](auto s){return s.Usable();},"Valid next sequence did not recover");
    HANDLE m=OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,room_surface::detail::Name(c.channel,true).c_str());
    HANDLE mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,room_surface::detail::Name(c.channel,false).c_str());
    auto* view=static_cast<uint8_t*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,room_surface::HEADER_BYTES+4096));
    Check(m&&mapping&&view,"Invalid-header fixture could not map channel");
    {room_surface::detail::Lock guard(m,100);auto* h=reinterpret_cast<room_surface::Header*>(view);h->stride=1;++h->sequence;}
    Wait(source,[](auto s){return s.state==State::Invalid;},"Malformed wire stride accepted");
    UnmapViewOfFile(view);CloseHandle(mapping);CloseHandle(m);
    Check(publisher.PublishBGRA(p.data(),p.size(),4,4,16),"Header recovery publish failed");
    Wait(source,[](auto s){return s.Usable();},"Header recovery failed");
}
static void TimestampOnlyHeartbeat()
{
    auto c=Configuration(std::to_string(GetCurrentProcessId())+"-heartbeat");
    room_surface::Publisher publisher(c.channel,1024);auto source=Source::Acquire(c);auto p=Pixels(4,4,17);
    Check(publisher.PublishBGRA(p.data(),p.size(),4,4,16),"Heartbeat fixture publish failed");
    auto first=Wait(source,[](auto s){return s.Usable();},"Heartbeat fixture absent");
    const auto original=first.frame;
    const auto* pixels=original->image.constBits();const auto key=original->image.cacheKey();
    HANDLE mutex=OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,room_surface::detail::Name(c.channel,true).c_str());
    HANDLE mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,room_surface::detail::Name(c.channel,false).c_str());
    auto* header=static_cast<room_surface::Header*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,room_surface::HEADER_BYTES+1024));
    Check(mutex&&mapping&&header,"Heartbeat fixture mapping failed");
    auto refresh=[&] {
        room_surface::detail::Lock guard(mutex,100);Check(guard.acquired,"Heartbeat fixture lock failed");
        header->timestamp_ms=GetTickCount64();return header->timestamp_ms;
    };
    std::uint64_t stamp=original->timestamp_ms;
    for(unsigned i=0;i<9;++i)
    {
        std::this_thread::sleep_for(250ms);stamp=refresh();
        auto alive=Wait(source,[&](auto s){return s.Usable()&&s.frame->timestamp_ms==stamp;},"Timestamp-only heartbeat did not renew snapshot");
        Check(alive.state==State::Static,"Timestamp-only heartbeat was treated as new pixels");
        Check(alive.frame!=original&&alive.frame->sequence==original->sequence&&alive.frame->generation==original->generation,"Heartbeat altered immutable metadata identity");
        Check(alive.frame->image.constBits()==pixels&&alive.frame->image.cacheKey()==key,"Heartbeat copied pixels or invalidated upload cache");
        Check(original->timestamp_ms==first.frame->timestamp_ms&&original->expires<alive.frame->expires,"Heartbeat mutated an already published snapshot");
    }
    Check(stamp-original->timestamp_ms>2*c.ttl_ms,"Heartbeat test did not exceed two TTL windows");
    Check(!first.Usable()&&source->Read().Usable(),"Retained snapshot was renewed or current snapshot expired");
    auto stable=source->Read();std::this_thread::sleep_for(30ms);
    auto unchanged=source->Read();
    Check(stable.frame==unchanged.frame&&stable.revision==unchanged.revision,"Polling unchanged timestamp republished metadata");
    Wait(source,[](auto s){return s.state==State::Stale;},"Stopped heartbeat remained live");
    Check(!source->Read().frame&&!stable.Usable(),"Stopped heartbeat exposed a stale renderable image");
    Check(GetTickCount64()-stamp<c.ttl_ms+200,"TTL was extended from poll time instead of header age");
    stamp=refresh();
    auto resumed=Wait(source,[&](auto s){return s.Usable()&&s.frame->timestamp_ms==stamp;},"Same-sequence heartbeat did not recover after expiry");
    Check(resumed.frame->sequence==original->sequence&&resumed.frame->image.constBits()==pixels&&resumed.frame->image.cacheKey()==key,"Heartbeat recovery recopied static pixels");
    UnmapViewOfFile(header);CloseHandle(mapping);CloseHandle(mutex);
}
static int ChildPublisher(const std::string& channel)
{
    room_surface::Publisher publisher(channel,1024);auto p=Pixels(4,4,42);
    if(!publisher.IsOpen()||!publisher.PublishBGRA(p.data(),p.size(),4,4,16))return 3;
    std::this_thread::sleep_for(10s);return 0;
}
static void ProducerDeath()
{
    auto c=Configuration(std::to_string(GetCurrentProcessId())+"-death");c.ttl_ms=5000;
    auto source=Source::Acquire(c);wchar_t executable[32768]{};
    Check(GetModuleFileNameW(nullptr,executable,32768)>0,"Test executable path absent");
    std::wstring command=L"\""+std::wstring(executable)+L"\" --publisher "+std::wstring(c.channel.begin(),c.channel.end());
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    Check(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=FALSE,"Synthetic child publisher failed");
    CloseHandle(process.hThread);
    try
    {
        auto before=Wait(source,[](auto s){return s.Usable();},"Child frame absent");
        Check(qBlue(before.frame->image.pixel(0,0))==42,"Child pixels incorrect");
        Check(TerminateProcess(process.hProcess,0)!=FALSE,"Own synthetic child did not terminate");
        Check(WaitForSingleObject(process.hProcess,2000)==WAIT_OBJECT_0,"Child did not exit");
        auto gone=Wait(source,[](auto s){return s.state==State::Unavailable;},"Producer death did not invalidate snapshot before TTL");
        Check(!gone.Usable()&&!gone.frame,"Dead producer remained usable");
        room_surface::Publisher replacement(c.channel,1024);auto p=Pixels(4,4,43);
        Check(replacement.IsOpen()&&replacement.PublishBGRA(p.data(),p.size(),4,4,16),"Replacement after death failed");
        Check(Wait(source,[&](auto s){return s.Usable()&&s.frame->generation!=before.frame->generation;},"Death/reconnect generation failed").frame->sequence==1,"New producer sequence was stale");
    }
    catch(...){TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,2000);CloseHandle(process.hProcess);throw;}
    CloseHandle(process.hProcess);
}
#endif
int main(int argc,char** argv)
{
    try
    {
#if defined(_WIN32) && !defined(SCREEN_SOURCES_DISABLE_FRAME_SURFACE)
        if(argc==3&&std::string(argv[1])=="--publisher")return ChildPublisher(argv[2]);
#else
        (void)argc;(void)argv;
#endif
        ConfigTests();
#if defined(_WIN32) && !defined(SCREEN_SOURCES_DISABLE_FRAME_SURFACE)
        Lifecycle();ExpiryAndContention();InvalidAndRecover();TimestampOnlyHeartbeat();ProducerDeath();
#else
        auto source=Source::Acquire(Configuration("unsupported"));Check(source->Read().state==State::Unsupported,"Unsupported backend not reported");Check(!source->Read().Usable(),"Unsupported backend exposed a frame");
#endif
        std::cout<<"PASS "<<assertions<<" assertions; synthetic frames only, no screen/app/hardware access\n";return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
