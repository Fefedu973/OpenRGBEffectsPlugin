// SPDX-License-Identifier: GPL-2.0-or-later
#include "../../Effects/IntelligentAmbience/VideoEngine.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace room_ai;
namespace
{
unsigned checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
double Energy(Color c){return c.r+c.g+c.b;}
bool Near(Color a,Color b,double tolerance=1e-5){return std::abs(a.r-b.r)+std::abs(a.g-b.g)+std::abs(a.b-b.b)<tolerance;}
VideoFrame Frame(unsigned n,double x=-100,unsigned width=128,unsigned height=72,Color encoded={1,.10f,.02f},double center_y=.51)
{
    auto rgb=std::make_shared<std::vector<std::uint8_t>>(std::size_t(width)*height*3,std::uint8_t(0));
    for(unsigned y=0;y<height;++y)for(unsigned px=0;px<width;++px)
    {
        const double xx=(px+.5)/width, yy=(y+.5)/height;
        if(std::abs(xx-x)<.068&&std::abs(yy-center_y)<.12)
        {
            // Textured translating rectangle: features move with the object.
            const double factor=(int((xx-x+.068)*width/4)+int(y/4))%2?.72:1.;
            const auto i=(std::size_t(y)*width+px)*3;
            (*rgb)[i]=std::uint8_t(encoded.r*255*factor);(*rgb)[i+1]=std::uint8_t(encoded.g*255*factor);(*rgb)[i+2]=std::uint8_t(encoded.b*255*factor);
        }
    }
    return {width,height,rgb,{1,1,0,n,n/30.}};
}
VideoFrame Solid(unsigned n,unsigned char r,unsigned char g,unsigned char b)
{
    auto f=Frame(n);auto p=std::make_shared<std::vector<std::uint8_t>>(f.rgb->size());
    for(std::size_t i=0;i<p->size();i+=3){(*p)[i]=r;(*p)[i+1]=g;(*p)[i+2]=b;}
    f.rgb=p;return f;
}
double PeripheralRed(const VideoEngine&e,double now)
{
    double peak=0;
    for(int x=0;x<45;++x)for(int y=0;y<9;++y)peak=std::max(peak,double(e.Sample({1.01+x*.01,.23+y*.012},now,true).r));
    return peak;
}
void Seed(VideoEngine&e,unsigned count=34)
{for(unsigned n=0;n<count;++n)Check(e.Push(Frame(n,.65+n*.015)),"moving frame accepted");}
void Basic()
{
    VideoEngine e;
    Check(Energy(e.Sample({.5,.2},0,true))==0,"uninitialized black");
    Check(e.Push(Solid(0,128,64,0)),"first frame accepted");
    const Color expected{Decode(128/255.f),Decode(64/255.f),0};
    Check(Near(e.Sample({.5,.2},0,false),expected),"sRGB decoded exactly once");
    Check(Near(e.Sample({-1,2},0,false),expected),"classical border clamping");
    Check(Near(e.Sample({.5,.2},0,true),expected),"observed pixels unchanged in predictive mode");
    Check(Energy(e.Sample({.5,.2},.751,true))==0,"stale input becomes black");
    Check(Energy(e.Sample({.5,.2},-1,true))==0,"future frame not rendered");
    Check(!e.Push(Solid(0,255,255,255)),"duplicate sequence rejected");
    auto bad=Solid(1,1,2,3);bad.stamp.source_time=0;Check(!e.Push(bad),"nonmonotonic timestamp rejected");
    bad.stamp.source_time=std::numeric_limits<double>::quiet_NaN();Check(!e.Push(bad),"NaN timestamp rejected");
    bad=Solid(1,1,2,3);bad.width=9000;Check(!e.Push(bad),"oversized dimensions rejected before reads");
    bad=Solid(1,1,2,3);bad.rgb=std::make_shared<const std::vector<std::uint8_t>>(3);Check(!e.Push(bad),"truncated buffer rejected");
    Check(e.Stats().rejected_frames==5,"rejection counter exact");
    Check(Energy(e.Sample({std::numeric_limits<double>::infinity(),0},0,true))==0,"invalid query safe");
    auto alias=std::make_shared<std::vector<std::uint8_t>>(128*72*3,std::uint8_t(255));
    VideoFrame f{128,72,alias,{2,1,0,0,1}};Check(e.Push(f),"new epoch accepts restarted sequence");
    alias->assign(alias->size(),0);Check(e.Sample({.5,.2},1,false).r==1,"engine does not retain mutable caller pixels");
    e.Reset();Check(e.Stats().event_count==0&&Energy(e.Sample({1.1,.2},1,true))==0,"reset clears all temporal state");
}
void Motion()
{
    VideoEngine e;e.SetPersistence(.6);e.SetStrength(1);Seed(e);
    Check(e.Stats().event_count>0&&e.Stats().event_count<=128,"outgoing movement creates bounded memory");
    const double now=33/30.;
    Check(Energy(e.Sample({1.1,.287},now,false))==0,"classical baseline black after object exits");
    const double peak=PeripheralRed(e,now);std::cout<<"trajectory_peak="<<peak<<" events="<<e.Stats().event_count<<'\n';
    Check(peak>.02,"predicted object continues beyond visible screen");
    Check(Energy(e.Sample({.5,.3},now,true))==0,"history never overwrites observed black inside screen");
    const auto before=e.Sample({1.14,.29},now,true);
    const auto batch=e.Sample(std::vector<Vec2>{{1.14,.29},{2.,.1},{1.14,.29}},now,true);
    Check(Near(before,batch[0])&&Near(batch[0],batch[2]),"sampling and LED density do not mutate field");
    for(unsigned n=34;n<100;++n)Check(e.Push(Frame(n)),"continuing black source accepted");
    Check(e.Stats().event_count==0&&PeripheralRed(e,99/30.)==0,"TTL expires while fresh black source continues");
    Check(!e.Push(Frame(0,.65)),"old timestamp does not replay an old trajectory");
}
void StationaryAndCuts()
{
    VideoEngine stationary;
    for(unsigned n=0;n<50;++n)Check(stationary.Push(Frame(n,.95)),"static HUD frame");
    Check(stationary.Stats().event_count==0,"stationary bright HUD emits no events");
    VideoEngine e;e.SetPersistence(1);Seed(e);
    const auto remembered=e.Stats().event_count;
    Check(e.Push(Solid(34,255,255,255)),"flash frame accepted");
    Check(e.Push(Frame(35)),"flash returns to black scene");
    Check(!e.Stats().cut&&e.Stats().event_count==remembered,"one-frame white flash preserves memory");
    Check(e.Push(Solid(36,0,0,255)),"cut candidate accepted");
    Check(e.Sample({1.1,.28},36/30.,true).r==0,"possible cut immediately suppresses red history");
    Check(e.Push(Solid(37,0,0,255)),"cut confirmation accepted");
    Check(e.Stats().cut&&e.Stats().event_count==0,"persistent global cut clears memory");
    VideoEngine persistent_white;Seed(persistent_white);
    Check(persistent_white.Push(Solid(34,255,255,255))&&persistent_white.Push(Solid(35,255,255,255)),"persistent white frames");
    Check(persistent_white.Stats().cut&&persistent_white.Stats().event_count==0,"persistent white is a cut not an eternal flash");
}
void Discontinuities()
{
    VideoEngine e;Seed(e);
    auto f=Frame(34);f.stamp.layout_revision=123;
    Check(e.Push(f)&&e.Stats().event_count>0,"layout revision alone preserves video memory");
    f=Frame(35);f.stamp.stream_epoch=2;f.stamp.sequence=0;f.stamp.source_time=.1;
    Check(e.Push(f)&&e.Stats().event_count==0,"stream restart clears memory and allows new timebase");
    Check(!e.Push(Frame(99)),"delayed old stream epoch rejected");
    e.Reset();Seed(e);f=Frame(34);f.stamp.state_epoch=2;
    Check(e.Push(f)&&e.Stats().event_count==0,"state discontinuity resets memory");
    e.Reset();Seed(e);f=Frame(100);
    Check(e.Push(f)&&e.Stats().event_count==0,"long capture interruption prevents extrapolating old flow");
    e.Reset();Seed(e);f=Frame(34,-100,160,90);
    Check(e.Push(f)&&e.Stats().event_count==0,"resolution change resets memory");
    e.Reset();Seed(e);e.SetScreenHeight(1);
    Check(e.Stats().event_count==0,"screen geometry change invalidates memory");
    Check(e.Push(Solid(0,128,128,128)),"new geometric source accepted");
    e.SetStrength(std::numeric_limits<double>::quiet_NaN());e.SetPersistence(-100);e.SetScreenHeight(0);
    Check(std::isfinite(e.Sample({1.1,.2},0,true).r),"invalid settings cannot poison output");
    VideoEngine skip;
    for(unsigned n=0;n<32;n+=2)Check(skip.Push(Frame(n,.65+n*.015)),"skipped frames use real timestamps");
    Check(skip.Stats().event_count>0,"motion still detected across bounded skipped frames");
    VideoEngine tiny;Check(tiny.Push(Frame(0,.94)),"tiny-dt initial source");
    f=Frame(1,.955);f.stamp.source_time=1e-320;
    Check(tiny.Push(f)&&tiny.Stats().event_count==0&&std::isfinite(tiny.Stats().camera_velocity.x),"submicrosecond dt resets motion before division");
}
void CameraTranslation()
{
    VideoEngine e;
    for(unsigned n=0;n<4;++n)
    {
        auto f=Frame(n);auto p=std::make_shared<std::vector<std::uint8_t>>(f.rgb->size());
        for(unsigned y=0;y<f.height;++y)for(unsigned x=0;x<f.width;++x)
        {
            // Deterministic nonperiodic full-frame texture, translated two pixels.
            const unsigned sx=(x+256-2*n)/2,sy=y/2;
            const unsigned char v=std::uint8_t(20+((sx*73u+sy*37u+(sx*sy*13u))%180));
            const auto i=(std::size_t(y)*f.width+x)*3;(*p)[i]=(*p)[i+1]=(*p)[i+2]=v;
        }
        f.rgb=p;Check(e.Push(f),"translated texture accepted");
    }
    const auto stats=e.Stats();
    Check(stats.camera_confidence>.7,"global translation requires broad coherent support");
    Check(std::abs(stats.camera_velocity.x-2.*30/128)<.03&&std::abs(stats.camera_velocity.y)<.03,"global translation retains physical timestamp units");
}
void OtherEdges()
{
    for(unsigned edge=0;edge<3;++edge)
    {
        VideoEngine e;e.SetScreenHeight(1);e.SetStrength(1);
        for(unsigned n=0;n<38;++n)
        {
            const double travel=.35-n*.015;
            const double x=edge==0?travel:.51,y=edge==1?travel:edge==2?1-travel:.51;
            e.Push(Frame(n,x,128,128,{.1f,1,.05f},y));
        }
        double peak=0;
        for(int along=0;along<13;++along)for(int out=0;out<25;++out)
        {
            const double a=.39+along*.02,b=.01+out*.01;
            const Vec2 p=edge==0?Vec2{-b,a}:edge==1?Vec2{a,-b}:Vec2{a,1+b};
            peak=std::max(peak,double(e.Sample(p,37/30.,true).g));
        }
        Check(peak>.01,"outgoing memory works on left/top/bottom, without stretching world geometry");
    }
}
void BoundsAndBenchmark()
{
    VideoEngine e;std::vector<double> timings;timings.reserve(180);
    bool bounded=true;
    for(unsigned n=0;n<180;++n)
    {
        auto f=Frame(n,.65+(n%36)*.015,160,90);
        e.Push(f);timings.push_back(e.Stats().frame_ms);
        const auto c=e.Sample({1.1,.28},f.stamp.source_time,true);
        bounded &= e.Stats().event_count<=128&&std::isfinite(c.r)&&c.r>=0&&c.r<=1&&c.g>=0&&c.g<=1&&c.b>=0&&c.b<=1;
    }
    Check(bounded,"long sequence has bounded finite output and memory");
    std::sort(timings.begin(),timings.end());
    const auto start=std::chrono::steady_clock::now();double checksum=0;
    for(int repeat=0;repeat<100;++repeat)for(int i=0;i<2048;++i)checksum+=e.Sample({-.2+(i%64)*.022,.01+(i/64)*.02},179/30.,true).r;
    const auto sample_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/100.;
    Check(std::isfinite(checksum),"batch stress finite");
    std::cout<<"benchmark frame160x90_n180 median_ms="<<timings[90]<<" p95_ms="<<timings[171]<<" max_ms="<<timings.back()<<" sample2048_ms="<<sample_ms<<'\n';
    std::vector<double> dense;
    auto f=Frame(0,-100,1920,1080);auto pixels=std::make_shared<std::vector<std::uint8_t>>(f.rgb->size());
    for(unsigned y=0;y<f.height;++y)for(unsigned x=0;x<f.width;++x)
    {
        const unsigned char v=std::uint8_t(20+((x/30*73u+y/30*37u+(x/30)*(y/30)*13u)%180));
        const auto i=(std::size_t(y)*f.width+x)*3;(*pixels)[i]=(*pixels)[i+1]=(*pixels)[i+2]=v;
    }
    f.rgb=pixels;VideoEngine textured;
    for(unsigned n=0;n<60;++n){f.stamp.sequence=n;f.stamp.source_time=n/30.;textured.Push(f);if(n)dense.push_back(textured.Stats().frame_ms);}
    std::sort(dense.begin(),dense.end());
    Check(textured.Stats().event_count==0,"dense stationary texture does not create events");
    std::cout<<"benchmark frame1920x1080_textured_n59 median_ms="<<dense[dense.size()/2]<<" p95_ms="<<dense[55]<<" max_ms="<<dense.back()<<'\n';
}
}
int main()
{
    try{Basic();Motion();StationaryAndCuts();Discontinuities();CameraTranslation();OtherEdges();BoundsAndBenchmark();std::cout<<"PASS "<<checks<<" assertions\n";return 0;}
    catch(const std::exception&e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
}
