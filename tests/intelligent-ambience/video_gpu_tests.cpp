// SPDX-License-Identifier: GPL-2.0-or-later
// Synthetic inputs only. Actual production ShaderProgram/DynamicShaderImage,
// no screen capture, plugin host, controller, microphone or device discovery.
#include "Effects/IntelligentAmbience/VideoEngine.h"
#include "Effects/Shaders/ShaderProgram.h"
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QFile>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <array>

using namespace room_ai;
namespace
{
unsigned checks=0,comparisons=0;int maximum_error=0;
void Check(bool value,const char*message){++checks;if(!value)throw std::runtime_error(message);}
VideoFrame Solid(unsigned sequence,unsigned char r,unsigned char g,unsigned char b)
{
    auto pixels=std::make_shared<std::vector<std::uint8_t>>(128*72*3);
    for(std::size_t i=0;i<pixels->size();i+=3){(*pixels)[i]=r;(*pixels)[i+1]=g;(*pixels)[i+2]=b;}
    return {128,72,pixels,{1,1,0,sequence,sequence/30.}};
}
VideoFrame Moving(unsigned n)
{
    auto f=Solid(n,0,0,0);auto pixels=std::make_shared<std::vector<std::uint8_t>>(*f.rgb);
    const double center=.65+n*.015;
    for(unsigned y=0;y<f.height;++y)for(unsigned x=0;x<f.width;++x)
    {
        const double xx=(x+.5)/f.width, yy=(y+.5)/f.height;
        if(std::abs(xx-center)<.068&&std::abs(yy-.51)<.12)
        {
            const double gain=(int((xx-center+.068)*f.width/4)+int(y/4))%2?.72:1.;
            const auto i=(std::size_t(y)*f.width+x)*3;
            (*pixels)[i]=std::uint8_t(255*gain);(*pixels)[i+1]=std::uint8_t(25*gain);(*pixels)[i+2]=std::uint8_t(5*gain);
        }
    }
    f.rgb=pixels;return f;
}
void Seed(VideoEngine&e){bool accepted=true;for(unsigned n=0;n<34;++n)accepted&=e.Push(Moving(n));Check(accepted,"trajectory input accepted");}
std::shared_ptr<const DynamicShaderImage> Texture(const VideoTexture&t,std::uint64_t revision)
{
    auto result=std::make_shared<DynamicShaderImage>();result->width=t.width;result->height=t.height;
    result->rgba32f=t.rgba;result->sequence=revision;result->generation=1;return result;
}
Uniforms Bind(const std::shared_ptr<const VideoRenderState>&s,double now,bool predictive,std::array<float,4>screen)
{
    Uniforms u;u.images[0]=Texture(s->grid,s->revision);u.images[1]=Texture(s->events,s->revision);
    u.custom["iaScreen"]={screen,4};
    auto scalar=[&](const char*key,double value){u.custom[key]={{{float(value),0,0,0}},1};};
    scalar("iaAge",now-s->source_time);scalar("iaSourceAge",now-s->alive_time);
    scalar("iaScreenHeight",s->screen_height);scalar("iaPersistence",s->persistence);scalar("iaStrength",s->strength);
    scalar("iaPredictive",predictive);scalar("iaValid",s->valid);scalar("iaSuppress",s->suppress_prediction);scalar("iaEventCount",s->event_count);
    return u;
}
void Compare(ShaderProgram& program,QOpenGLFunctions*gl,const VideoEngine&e,double now,bool predictive,
             const char*label,std::array<float,4>screen={80,55,160,90})
{
    auto state=e.RenderState();auto u=Bind(state,now,predictive,screen);
    program.Draw(u,gl);const auto image=program.Image();int error=0;double total=0;unsigned samples=0;
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)
    {
        Vec2 world{((x+.5)*320/image.width()-screen[0])/screen[2],
                   ((y+.5)*200/image.height()-screen[1])/screen[3]*state->screen_height};
        const auto reference=e.Sample(world,now,predictive);const auto actual=image.pixelColor(x,y);
        const std::array<int,3> expected={int(std::lround(Encode(reference.r)*255)),int(std::lround(Encode(reference.g)*255)),int(std::lround(Encode(reference.b)*255))};
        const std::array<int,3> observed={actual.red(),actual.green(),actual.blue()};
        for(unsigned c=0;c<3;++c){const int d=std::abs(expected[c]-observed[c]);error=std::max(error,d);total+=d;++samples;}
    }
    ++comparisons;maximum_error=std::max(maximum_error,error);
    std::cout<<label<<": max_byte_error="<<error<<" mae="<<total/samples<<'\n';
    Check(error<=2,"GPU differs from CPU Sample by more than two sRGB8 levels");
}
}
int main(int argc,char**argv)
{
    QGuiApplication application(argc,argv);
    try
    {
        Check(argc==2,"pass shaders/IntelligentAmbience/video.fs as the sole argument");
        QFile file(QString::fromLocal8Bit(argv[1]));Check(file.open(QIODevice::ReadOnly),"video shader readable");
        const auto source=file.readAll().toStdString();
        QOffscreenSurface surface;surface.create();QOpenGLContext context;context.setFormat(surface.format());
        Check(context.create()&&context.makeCurrent(&surface),"offscreen GL context unavailable");
        auto*gl=context.functions();
        ShaderProgram program;program.SetVersion("130");program.Resize(320,200);
        for(int i=0;i<2;++i){auto*input=new ShaderPass(ShaderPass::DYNAMIC_IMAGE);input->data.image_slot=i;program.passes.push_back(input);}
        program.main_pass->data.fragment_shader=source+"\nvoid mainImage(out vec4 c,in vec2 p){iaVideoImage(c,p);}\n";
        program.Init();const auto errors=program.Compile();
        if(!errors.isEmpty())std::cerr<<errors.toStdString()<<'\n';Check(errors.isEmpty(),"video GLSL130 compiles in production ShaderProgram");

        VideoEngine empty;Compare(program,gl,empty,0,true,"empty");
        Check(!empty.Refresh(0),"cannot declare liveness before any observation");
        VideoEngine e;Check(e.Push(Solid(0,128,64,2)),"solid accepted");
        Check(std::abs(e.Sample({.2,.2},0,false).r-Decode(128/255.f))<1e-6,"reference linear light");
        const auto first=e.RenderState();Check(first==e.RenderState(),"unchanged numeric snapshot is cached");
        Compare(program,gl,e,0,false,"linear SDR baseline");Compare(program,gl,e,0,true,"solid predictive");
        const auto uploads=program.passes[0]->ImageUploads();Compare(program,gl,e,.1,false,"same pixels new age");
        Check(program.passes[0]->ImageUploads()==uploads,"relative age does not cause pixel uploads");
        Compare(program,gl,e,.751,true,"stale source black");Compare(program,gl,e,-.01,true,"future observation black");
        Check(!e.Refresh(-.1)&&!e.Refresh(std::numeric_limits<double>::infinity()),"invalid heartbeat rejected");
        Check(e.Refresh(1)&&e.Refresh(2)&&e.Refresh(3),"static producer heartbeat accepted");
        const auto alive=e.RenderState();
        Check(alive!=first&&alive->grid.rgba==first->grid.rgba&&alive->events.rgba==first->events.rgba&&alive->revision==first->revision,"heartbeat clones metadata without pixels or upload revision");
        Check(alive->source_time==0&&alive->alive_time==3&&first->alive_time==0,"observation clock and immutable liveness are distinct");
        Compare(program,gl,e,3,true,"static image alive after four stale intervals");
        Check(program.passes[0]->ImageUploads()==uploads,"heartbeat does not upload unchanged pixels");
        Compare(program,gl,e,2.9,true,"future liveness rejected by sampling");
        Compare(program,gl,e,3.751,true,"heartbeat stopped becomes stale");

        auto gradient=Solid(0,0,0,0);auto pixels=std::make_shared<std::vector<std::uint8_t>>(*gradient.rgb);
        for(unsigned y=0;y<gradient.height;++y)for(unsigned x=0;x<gradient.width;++x)
        {const auto n=(std::size_t(y)*gradient.width+x)*3;(*pixels)[n]=std::uint8_t(x*255/(gradient.width-1));(*pixels)[n+1]=std::uint8_t(y*255/(gradient.height-1));(*pixels)[n+2]=(x<64&&y<36)?255:0;}
        gradient.rgb=pixels;e.Reset();Check(e.Push(gradient),"asymmetric source accepted");
        Compare(program,gl,e,0,false,"orientation and bilinear texels");
        Compare(program,gl,e,0,false,"custom screen reference",{12,18,240,110});
        e.SetScreenHeight(.3);Check(e.Push(gradient),"aspect reset followed by source accepted");
        Compare(program,gl,e,0,true,"aspect and partially offcanvas reference",{-40,80,190,60});
        e.Reset();Compare(program,gl,e,0,true,"reset invalidates exported state");

        e.SetScreenHeight(.5625);e.SetPersistence(.6);e.SetStrength(1);Seed(e);
        Check(e.Stats().event_count>0,"moving object creates real state");
        const auto trajectory=e.RenderState();
        Check(trajectory->grid.width<=64&&trajectory->grid.height<=64&&trajectory->events.width==3&&trajectory->events.height==128,"export dimensions bounded");
        bool all_finite=true;for(float f:*trajectory->events.rgba)all_finite&=std::isfinite(f);Check(all_finite,"all numeric event fields finite");
        for(unsigned which=0;which<3;++which)
        {
            auto frozen=Bind(trajectory,1.1,true,{80,55,160,90});
            for(unsigned plane=0;plane<2;++plane)if(which==2||which==plane)
            {
                auto expired=std::make_shared<DynamicShaderImage>(*frozen.images[plane]);
                expired->expires=std::chrono::steady_clock::now()-std::chrono::seconds(1);frozen.images[plane]=expired;
            }
            // Keep iaValid, iaAge and iaEventCount unchanged: emulate a stalled
            // effect worker while the GL renderer continues independently.
            program.Draw(frozen,gl);const auto image=program.Image();bool black=true;
            for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)
            {const auto c=image.pixelColor(x,y);black&=c.red()==0&&c.green()==0&&c.blue()==0;}
            Check(black,"expired numeric plane must render black with frozen valid uniforms");
        }
        Compare(program,gl,e,1.1,true,"prediction at last observation");Compare(program,gl,e,1.25,true,"prediction advances");Compare(program,gl,e,1.6,true,"prediction ages");
        for(double strength:{0.,.35,1.}){e.SetStrength(strength);Compare(program,gl,e,1.25,true,"strength parameter");}
        for(double ttl:{.1,1.5,5.}){e.SetPersistence(ttl);Compare(program,gl,e,1.7,true,"persistence parameter");}
        e.SetPersistence(.6);Check(e.Refresh(4),"event source heartbeat without new observation");
        Compare(program,gl,e,4,true,"heartbeat never renews event TTL");
        Check(e.Sample({1.15,.285},4,true).r==0,"event expired despite live static source");
        Check(trajectory->source_time==1.1&&trajectory->alive_time==1.1&&trajectory->event_count>0,"older exported snapshot not mutated");

        e.Reset();Seed(e);Check(e.Push(Solid(34,0,0,255)),"cut candidate accepted");
        Check(e.RenderState()->suppress_prediction,"cut suppression is exported");Compare(program,gl,e,34/30.,true,"possible cut suppresses stale events");
        Check(e.Push(Solid(35,0,0,255))&&e.Stats().cut,"cut confirmed");Compare(program,gl,e,35/30.,true,"confirmed cut");
        auto next=Solid(0,0,255,0);next.stamp.stream_epoch=2;Check(e.Push(next),"source epoch restarts sequence");
        Compare(program,gl,e,0,true,"new stream epoch");
        Check(!e.Push(Solid(99,255,0,0)),"old stream epoch rejected");
        program.Resize(800,500);Compare(program,gl,e,0,true,"output resolution independent");
        Check(gl->glGetError()==GL_NO_ERROR,"no OpenGL error");program.CleanupGL();
        std::cout<<"PASS "<<checks<<" checks, "<<comparisons<<" full-frame CPU/GPU comparisons; max_sRGB8_error="<<maximum_error<<'\n';return 0;
    }
    catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
