// SPDX-License-Identifier: GPL-2.0-or-later
// Production adapters, synthetic owned images/PCM only. No model, capture or GPU.
#include "ModelInputs.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace room_ai::inference;
namespace
{
unsigned checks=0;
void Check(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
bool Near(double a,double b,double eps=1e-6){return std::abs(a-b)<eps;}
TensorSpec Spec(const char* name,std::initializer_list<std::int64_t> shape){return {name,shape};}
ModelConfig Video(int w=2,int h=2)
{
    ModelConfig c;c.id="test";c.task="video-field-v1";c.max_age_ms=250;
    c.inputs={Spec("current_rgb",{1,3,h,w}),Spec("previous_rgb",{1,3,h,w}),Spec("delta_seconds",{1}),Spec("screen_rect",{1,4})};
    c.outputs={Spec("field_rgb",{1,3,h,w})};return c;
}
ModelConfig Audio(int samples=480)
{auto c=Video();c.task="audio-field-v1";c.inputs={Spec("audio_pcm",{1,1,samples})};return c;}
const Tensor& Input(const Request& request,const char* name)
{for(const auto& value:request.inputs)if(value.name==name)return value;throw std::runtime_error("Missing test tensor");}
QImage Patch()
{
    QImage image(2,2,QImage::Format_RGBA8888);image.setPixelColor(0,0,Qt::red);image.setPixelColor(1,0,Qt::green);
    image.setPixelColor(0,1,Qt::blue);image.setPixelColor(1,1,Qt::white);return image;
}
constexpr std::array<double,4> Rect{.25,.275,.5,.45};
void VideoTests()
{
    auto c=Video();VideoAdapter adapter;auto image=Patch();
    auto first=adapter.Make(c,image,3,7,1,100,Rect);Check(bool(first),"first frame accepted");
    Check(first->reset&&first->generation==3&&first->epoch==7&&first->sequence==1&&first->source_time==100,"first/reset identity exact");
    const std::vector<float> expected{1,0,0,1,0,1,0,1,0,0,1,1};
    Check(Input(*first,"current_rgb").values==expected,"NCHW and top-left orientation exact");
    Check(Input(*first,"previous_rgb").values==expected&&Input(*first,"delta_seconds").values==std::vector<float>{0},"first frame duplicates current without imaginary previous");
    for(unsigned i=0;i<4;++i)Check(Near(Input(*first,"screen_rect").values[i],Rect[i]),"explicit calibrated rectangle");
    Check(!adapter.Make(c,image,3,7,1,100.01,Rect),"duplicate sequence skipped");
    auto second=adapter.Make(c,image,3,7,2,100.02,Rect);Check(second&&!second->reset,"continuous frame retains state");
    Check(Near(Input(*second,"delta_seconds").values[0],.02),"delta from actual timestamps");
    Check(!adapter.Make(c,image,3,7,1,100.03,Rect),"late sequence skipped");
    auto next=adapter.Make(c,image,3,7,3,100.35,Rect);Check(next&&next->reset,"gap resets state");
    next=adapter.Make(c,image,4,7,1,100.36,Rect);Check(next&&next->reset,"model generation resets independently of sequence");
    next=adapter.Make(c,image,4,8,1,100.37,Rect);Check(next&&next->reset,"source epoch resets independently");
    auto moved=Rect;moved[0]+=.1;next=adapter.Make(c,image,4,8,2,100.38,moved);Check(next&&next->reset,"geometry resets previous");
    QImage large(4,4,QImage::Format_RGB32);large.fill(Qt::red);next=adapter.Make(c,large,4,8,3,100.39,moved);Check(next&&next->reset,"source resolution reset");
    next=adapter.Make(Video(1,1),large,4,8,4,100.40,moved);Check(next&&next->reset,"tensor resolution reset");
    adapter.Reset();QImage dark(2,2,QImage::Format_RGB32);dark.fill(Qt::black);
    Check(bool(adapter.Make(c,dark,1,1,1,10,Rect)),"dark first accepted");QImage white=dark;white.fill(Qt::white);
    next=adapter.Make(c,white,1,1,2,10.01,Rect);Check(next&&next->reset,"large scene cut resets recurrent state");
    Check(Input(*next,"current_rgb").values==Input(*next,"previous_rgb").values,"cut discards previous shot");
    Check(!adapter.Make(c,white,1,1,3,10.005,Rect),"backward timestamp rejected");
    next=adapter.Make(c,white,1,1,4,10.02,Rect);Check(next&&next->reset,"invalid continuity invalidates history");
    // Area reduction must average *linear* black/white, not decode an sRGB128 average.
    QImage two(2,1,QImage::Format_RGB32);two.setPixelColor(0,0,Qt::black);two.setPixelColor(1,0,Qt::white);
    auto reduced=adapter.Make(Video(1,1),two,2,1,1,20,Rect);Check(bool(reduced),"linear reduction accepted");
    for(float v:Input(*reduced,"current_rgb").values)Check(Near(v,.5),"linear-light area reduction");
    auto expanded=adapter.Make(Video(4,1),two,3,1,1,21,Rect);
    const auto& up=Input(*expanded,"current_rgb").values;Check(Near(up[0],0)&&Near(up[1],.25)&&Near(up[2],.75)&&Near(up[3],1),"linear upsampling pixel centers");
    QImage gamma(1,1,QImage::Format_RGB32);gamma.fill(QColor(128,128,128));auto decoded=adapter.Make(Video(1,1),gamma,4,1,1,22,Rect);
    Check(Near(Input(*decoded,"current_rgb").values[0],.2158605,1e-6),"sRGB decode applied once");
    QImage transparent(1,1,QImage::Format_ARGB32_Premultiplied);transparent.fill(QColor(255,0,0,128));
    auto alpha=adapter.Make(Video(1,1),transparent,5,1,1,23,Rect);Check(Near(Input(*alpha,"current_rgb").values[0],128./255.,1e-5),"premultiplied input unpremultiplies before linear black composition");
    for(int invalid=0;invalid<11;++invalid)
    {
        auto bad=c;auto rect=Rect;QImage frame=image;std::uint64_t gen=1,epoch=1,seq=1;double time=1;
        if(invalid==0)bad.inputs[0].shape={1,3,257,2};if(invalid==1)bad.inputs[1].shape={1,3,3,2};
        if(invalid==2)bad.inputs[2].shape={1,1};if(invalid==3)bad.inputs[3].name="wrong";
        if(invalid==4)rect[2]=0;if(invalid==5)rect[0]=std::numeric_limits<double>::quiet_NaN();
        if(invalid==6)frame=QImage();if(invalid==7)gen=0;if(invalid==8)epoch=0;if(invalid==9)seq=0;
        if(invalid==10)time=std::numeric_limits<double>::infinity();
        Check(!adapter.Make(bad,frame,gen,epoch,seq,time,rect),"invalid video contract fails closed");
    }
}
room_audio::PcmWindowSnapshot Window(unsigned n=480)
{
    room_audio::PcmWindowSnapshot w;w.status=room_audio::PcmWindowStatus::Ready;w.epoch=5;w.sequence=8;
    w.source_rate=44100;w.source_channels=8;w.source_end=100;w.captured_through=100.001;
    w.source_begin=w.source_end-double(n)/48000;w.mono.resize(n);
    for(unsigned i=0;i<n;++i)w.mono[i]=float(std::sin(double(i)*.02));return w;
}
void AudioTests()
{
    auto c=Audio();auto w=Window();AudioAdapter adapter;
    Check(AudioSamples(c)==480&&AudioSamples(Audio(240000))==240000,"audio contract sample bounds inclusive");
    Check(!AudioSamples(Video())&&!AudioSamples(Audio(479))&&!AudioSamples(Audio(240001)),"wrong task/sample count rejected");
    auto first=adapter.Make(c,w,2,100.005);Check(first&&first->reset,"first PCM request accepted/reset");
    Check(first->source_time==100&&first->epoch==5&&first->sequence==8,"PCM end/QPC and epoch exact");
    Check(Input(*first,"audio_pcm").values==w.mono,"entire PCM window copied unchanged");
    Check(!adapter.Make(c,w,2,100.01),"same capture sequence deduplicated");
    w.sequence++;w.source_begin+=.01;w.source_end+=.01;w.captured_through+=.01;
    auto next=adapter.Make(c,w,2,100.02);Check(next&&!next->reset,"overlapping new capture window preserves recurrence");
    w.epoch++;w.sequence=1;next=adapter.Make(c,w,2,100.02);Check(next&&next->reset,"PCM session reset");
    next=adapter.Make(c,w,3,100.02);Check(next&&next->reset,"PCM model reset");
    for(int invalid=0;invalid<11;++invalid)
    {
        adapter.Reset();auto bad=Window();double now=100.01;
        if(invalid==0)bad.status=room_audio::PcmWindowStatus::Stale;if(invalid==1)bad.sample_rate=44100;
        if(invalid==2)bad.mono.pop_back();if(invalid==3)bad.mono[1]=std::numeric_limits<float>::quiet_NaN();
        if(invalid==4)bad.mono[1]=1.01f;if(invalid==5)bad.epoch=0;if(invalid==6)bad.sequence=0;
        if(invalid==7)bad.source_begin+=.001;if(invalid==8)now=99.99;if(invalid==9)now=100.2;
        if(invalid==10)bad.captured_through=99.99;
        Check(!adapter.Make(c,bad,1,now),"invalid/stale/future PCM fails closed");
    }
    // Real ring/resampler with synthetic48k,44.1k and7.1-origin mono; no capture.
    for(unsigned rate:{44100u,48000u})
    {
        room_audio::PcmWindowBuffer buffer;std::vector<float> pcm(rate/5,.2f);
        Check(buffer.Append(pcm.data(),pcm.size(),rate,8,10),"synthetic full PCM packets admitted");
        const auto captured=buffer.Read(4800,10.2);adapter.Reset();auto request=adapter.Make(Audio(4800),captured,9,10.2);
        Check(bool(request),"real PCM resampler snapshot accepted");
        Check(request->source_time==captured.source_end,"resampler lookback timestamp not replaced by render time");
        Check(Input(*request,"audio_pcm").values.size()==4800,"all target samples passed");
        for(std::size_t i=0;i<4800;i+=157)Check(Near(Input(*request,"audio_pcm").values[i],.2,1e-4),"constant waveform preserved through adapter");
    }
}
Tensor Output(const TensorSpec& s,std::vector<float> v){Tensor t;t.name=s.name;t.shape=s.shape;t.values=std::move(v);return t;}
Result Field(const ModelConfig& c)
{
    Result r;r.generation=2;r.epoch=5;r.sequence=10;r.source_time=100;r.completed_time=100.01;r.expires=100.25;
    r.outputs.push_back(Output(c.outputs[0],{1,0,.25f,.5f,0,1,.5f,.25f,0,0,1,1}));return r;
}
void OutputTests()
{
    auto c=Video();auto result=Field(c);auto image=FieldImage(result,c,100.02);
    Check(image&&image->Valid()&&image->Width()==2&&image->Height()==2,"field dimensions valid");
    Check(image->generation==2&&image->source_revision==5&&image->sequence==10,"output identity maintained");
    Check(*image->rgba32f==std::vector<float>({1,0,0,1,0,1,0,1,.25,.5,1,1,.5,.25,1,1}),"planar RGB to straight RGBA exact top-left");
    const auto expiry=std::chrono::duration<double>(image->expires.time_since_epoch()).count();Check(Near(expiry,100.25),"absolute monotone TTL unchanged");
    c.outputs.push_back(Spec("confidence",{1,1,2,2}));result.outputs.push_back(Output(c.outputs[1],{.2f,-1,2,.7f}));
    result.outputs[0].values[0]=2;result.outputs[0].values[1]=-1;
    image=FieldImage(result,c,100.02);Check(bool(image),"finite excursions accepted for clamp");
    Check((*image->rgba32f)[0]==1&&(*image->rgba32f)[4]==0&&Near((*image->rgba32f)[3],.2)&&(*image->rgba32f)[7]==0&&(*image->rgba32f)[11]==1,"RGB/confidence clamp without premultiplication");
    auto saved=image;result.outputs[0].values[0]=0;Check((*saved->rgba32f)[0]==1,"published texture owns immutable copy");
    // Deterministic simulation: the asynchronous worker completes after Step's
    // first clock sample. Refreshing the consumer clock admits the same result;
    // no sleep, scheduling assumption, TTL extension or fabricated timestamp.
    auto asynchronous=result;asynchronous.completed_time=100.025;
    Check(!FieldImage(asynchronous,c,100.02),"completion after old Step clock cannot be read causally");
    auto after=FieldImage(asynchronous,c,100.03);
    Check(after&&Near(std::chrono::duration<double>(after->expires.time_since_epoch()).count(),100.25),"fresh read clock admits asynchronous result with original expiry");
    for(int invalid=0;invalid<13;++invalid)
    {
        auto bad=result;auto config=c;double now=100.02;
        if(invalid==0)bad.outputs[0].values[3]=std::numeric_limits<float>::infinity();
        if(invalid==1)bad.outputs[1].values[0]=std::numeric_limits<float>::quiet_NaN();
        if(invalid==2)bad.outputs[0].values.pop_back();if(invalid==3)bad.outputs[1].shape={1,1,1,4};
        if(invalid==4)bad.outputs.pop_back();if(invalid==5)bad.sequence=0;if(invalid==6)bad.generation=0;
        if(invalid==7)now=100.251;if(invalid==8)bad.completed_time=101;if(invalid==9)bad.source_time=100.03;
        if(invalid==10)config.field_rect[2]=0;if(invalid==11)config.outputs[0].shape={1,3,257,2};
        if(invalid==12)bad.expires=std::numeric_limits<double>::infinity();
        Check(!FieldImage(bad,config,now),"invalid field rejected entirely");
    }
    result=Field(Video());result.expires=999;Check(!FieldImage(result,Video(),100.3),"model lease cannot be extended by result");
}
}
int main()
{
    try{VideoTests();AudioTests();OutputTests();std::cout<<checks<<" model input/output production checks PASS; no capture/model/hardware\n";return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
}
