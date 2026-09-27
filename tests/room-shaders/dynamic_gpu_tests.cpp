// SPDX-License-Identifier: GPL-2.0-or-later
#include "ShaderProgram.h"
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <iostream>
#include <stdexcept>
#include <thread>
#include "ScreenSources/ScreenSource.h"
#include <FrameSurface/FrameSurface.h>
static unsigned checks=0;
static void Check(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
static bool Near(QColor a,QColor b){return std::abs(a.red()-b.red())<=1 && std::abs(a.green()-b.green())<=1 && std::abs(a.blue()-b.blue())<=1;}
int main(int argc,char** argv)
{
    QGuiApplication app(argc,argv);
    try
    {
        QOffscreenSurface surface;surface.create();QOpenGLContext context;context.setFormat(surface.format());
        Check(context.create()&&context.makeCurrent(&surface),"GL context unavailable");auto* gl=context.functions();
        ShaderProgram program;program.SetVersion("130");program.Resize(2,2);
        auto* input=new ShaderPass(ShaderPass::DYNAMIC_IMAGE);program.passes.push_back(input);
        program.main_pass->data.fragment_shader="void mainImage(out vec4 c,in vec2 p){c=texture2D(iChannel0,vec2(p.x/iResolution.x,1.-p.y/iResolution.y));}";
        program.Init();Check(program.Compile().isEmpty(),"compile");
        auto frame=std::make_shared<DynamicShaderImage>();frame->sequence=1;frame->generation=9;
        frame->image=QImage(2,2,QImage::Format_ARGB32);frame->image.setPixelColor(0,0,Qt::red);frame->image.setPixelColor(1,0,Qt::green);frame->image.setPixelColor(0,1,Qt::blue);frame->image.setPixelColor(1,1,Qt::white);
        Uniforms u;u.images[0]=frame;program.Draw(u,gl);auto image=program.Image();
        Check(Near(image.pixelColor(0,0),Qt::red),"top left / BGRA order");Check(Near(image.pixelColor(1,0),Qt::green),"top right");Check(Near(image.pixelColor(0,1),Qt::blue),"bottom left");Check(Near(image.pixelColor(1,1),Qt::white),"bottom right");
        for(int i=0;i<100;++i)program.Draw(u,gl);
        Check(input->ImageUploads()==1,"static frame uploaded repeatedly");
        auto wrapper=std::make_shared<DynamicShaderImage>(*frame);u.images[0]=wrapper;program.Draw(u,gl);Check(input->ImageUploads()==1,"new wrapper of identical frame uploaded");
        auto next=std::make_shared<DynamicShaderImage>(*frame);next->sequence=3;next->image.fill(Qt::yellow);u.images[0]=next;program.Draw(u,gl);
        Check(Near(program.Image().pixelColor(0,0),Qt::yellow),"new sequence not uploaded");
        auto numeric=std::make_shared<DynamicShaderImage>();numeric->sequence=4;numeric->width=numeric->height=1;
        numeric->rgba32f=std::make_shared<const std::vector<float>>(std::initializer_list<float>{.125f,.5f,.875f,1});u.images[0]=numeric;program.Draw(u,gl);
        Check(Near(program.Image().pixelColor(0,0),QColor(32,128,223)),"RGBA32F input or resize");
        const auto count=input->ImageUploads();program.Draw(u,gl);Check(input->ImageUploads()==count,"numeric static upload repeated");
        auto expired=std::make_shared<DynamicShaderImage>(*numeric);expired->expires=std::chrono::steady_clock::now()-std::chrono::seconds(1);u.images[0]=expired;program.Draw(u,gl);
        Check(Near(program.Image().pixelColor(0,0),Qt::black),"expired input retained old pixels");
        auto invalid=std::make_shared<DynamicShaderImage>(*numeric);invalid->width=4097;Check(!invalid->Valid(),"oversize accepted");invalid->width=2;Check(!invalid->Valid(),"short numeric payload accepted");
        u.images[0]=invalid;program.Draw(u,gl);Check(Near(program.Image().pixelColor(0,0),Qt::black),"invalid frame rendered");
        u.images[0]=frame;program.Draw(u,gl);Check(Near(program.Image().pixelColor(0,1),Qt::blue),"recovery failed");
        program.CleanupGL();program.Init();Check(program.Compile().isEmpty(),"re-init compile");program.Draw(u,gl);Check(Near(program.Image().pixelColor(1,0),Qt::green),"restart retained invalid GL handles");program.CleanupGL();

        ShaderProgram multi;multi.SetVersion("130");multi.Resize(8,6);
        auto* source=new ShaderPass(ShaderPass::DYNAMIC_IMAGE);source->data.image_slot=1;multi.passes.push_back(source);
        auto* reduce=new ShaderPass(ShaderPass::BUFFER);reduce->data.width=2;reduce->data.height=1;
        reduce->data.fragment_shader="void mainImage(out vec4 c,in vec2 p){c=vec4(iResolution.x/10.,iResolution.y/10.,texture2D(iChannel0,vec2(.5)).r,1);}";multi.passes.push_back(reduce);
        multi.main_pass->data.fragment_shader="void mainImage(out vec4 c,in vec2 p){c=texture2D(iChannel1,vec2(.5));}";
        multi.Init();Check(multi.Compile().isEmpty(),"multipass compile");u.images[1]=numeric;multi.Draw(u,gl);
        Check(Near(multi.Image().pixelColor(0,0),QColor(51,26,32)),"fixed resolution or input slot lost");
        auto encoded=multi.ToJSON();auto* decoded=ShaderProgram::FromJSON(encoded);Check(decoded->passes[0]->data.image_slot==1 && decoded->passes[1]->data.width==2,"pass serialization");delete decoded;
        auto excessive=encoded;while(excessive["passes"].size()<5)excessive["passes"].push_back(encoded["passes"][0]);
        bool rejected=false;try{delete ShaderProgram::FromJSON(excessive);}catch(const std::exception&){rejected=true;}Check(rejected,"excessive pass count accepted");
        auto malformed=encoded;malformed["passes"][0]["image_slot"]=-1;rejected=false;try{delete ShaderProgram::FromJSON(malformed);}catch(const std::exception&){rejected=true;}Check(rejected,"invalid texture slot accepted");
        multi.Resize(16,10);multi.Draw(u,gl);Check(multi.Image().size()==QSize(16,10) && Near(multi.Image().pixelColor(0,0),QColor(51,26,32)),"fixed pass resized with output");multi.CleanupGL();
#ifdef _WIN32
        // Integration across the actual wire reader and production GL passes,
        // using an isolated synthetic channel, never a desktop capture.
        screen_source::Config config;config.channel="shader-test-"+std::to_string(GetCurrentProcessId());config.poll_ms=5;
        auto publisher=std::make_unique<room_surface::Publisher>(config.channel,64);
        Check(publisher->IsOpen(),"synthetic publisher open");
        const std::uint8_t pixels[]={0,0,255,255,255,0,0,255};
        Check(publisher->PublishBGRA(pixels,sizeof(pixels),2,1,8),"synthetic wire publish");
        auto reader=screen_source::Source::Acquire(config);screen_source::Snapshot snapshot;
        for(unsigned i=0;i<200;++i){snapshot=reader->Read();if(snapshot.Usable())break;std::this_thread::sleep_for(std::chrono::milliseconds(5));}
        Check(snapshot.Usable(),"wire frame not received");
        auto wire=std::make_shared<DynamicShaderImage>();wire->image=snapshot.frame->image;wire->sequence=snapshot.frame->sequence;wire->generation=snapshot.frame->generation;wire->expires=snapshot.frame->expires;
        u.images[0]=wire;program.Init();Check(program.Compile().isEmpty(),"wire pipeline compile");program.Draw(u,gl);
        Check(Near(program.Image().pixelColor(0,0),Qt::red) && Near(program.Image().pixelColor(1,0),Qt::blue),"wire-to-GPU colors");
        publisher.reset();
        for(unsigned i=0;i<200;++i){snapshot=reader->Read();if(!snapshot.Usable())break;std::this_thread::sleep_for(std::chrono::milliseconds(5));}
        Check(!snapshot.Usable(),"closed wire still usable");u.images[0].reset();program.Draw(u,gl);
        Check(Near(program.Image().pixelColor(0,0),Qt::black),"closed wire pipeline not black");program.CleanupGL();reader.reset();
#endif
        Check(gl->glGetError()==GL_NO_ERROR,"GL error");
        std::cout<<"PASS "<<checks<<" dynamic texture checks\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
