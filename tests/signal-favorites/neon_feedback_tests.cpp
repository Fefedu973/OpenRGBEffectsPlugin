// SPDX-License-Identifier: GPL-2.0-or-later
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QColor>
#include <QElapsedTimer>
#include "ShaderProgram.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <cmath>
static unsigned checks=0;
static void Check(bool ok,const char* why) { ++checks; if(!ok) throw std::runtime_error(why); }
static void Near(int actual,int expected,const char* why) { Check(std::abs(actual-expected)<=3,why); }
static json Read(const QString& file) { std::ifstream in(file.toStdString());json j;in>>j;return j; }
static void Compile(ShaderProgram& p)
{
    auto log=p.Compile();
    // Compatibility-profile gl_FragColor may produce a deprecation warning;
    // accept that single known warning, while rejecting all compilation errors.
    if(!log.isEmpty() && (!log.contains("warning C7533") || log.contains("error",Qt::CaseInsensitive)))
        throw std::runtime_error(log.toStdString());
}
static QImage Draw(ShaderProgram& p,const Uniforms& u,QOpenGLFunctions* gl)
{
    p.Draw(u,gl);const auto image=p.Image();Check(!image.isNull(),"missing image");
    Check(gl->glGetError()==GL_NO_ERROR,"OpenGL error");return image;
}
static void FeedbackTests(QOpenGLFunctions* gl)
{
    ShaderProgram p;p.SetVersion("130");p.Resize(64,32);p.main_pass->data.feedback=true;
    p.main_pass->data.fragment_shader="void mainImage(out vec4 c,in vec2 p){vec4 old=texture2D(iPreviousFrame,p/iResolution.xy);c=vec4(old.r+0.125,old.a*0.25,0.0,1.0);}";
    const auto saved=p.main_pass->ToJSON();Check(saved.at("feedback")==true,"save feedback flag");
    auto* restored=ShaderPass::FromJSON(saved);Check(restored->data.feedback,"load feedback flag");delete restored;
    auto legacy=saved;legacy.erase("feedback");restored=ShaderPass::FromJSON(legacy);Check(!restored->data.feedback,"old profile opt-out");delete restored;
    auto* copy=p.main_pass->Copy();Check(copy->data.feedback,"copy feedback flag");delete copy;
    p.Init();Compile(p);Uniforms u;
    for(int i=1;i<=6;++i)
    {
        const auto image=Draw(p,u,gl);Near(qRed(image.pixel(24,12)),std::min(255,32*i),"ping-pong accumulated previous frame");
        Near(qGreen(image.pixel(24,12)),i==1?0:64,"history alpha is zero only before first draw");
    }
    // Recompile clears even if the caller has enabled a restrictive scissor.
    gl->glEnable(GL_SCISSOR_TEST);gl->glScissor(0,0,1,1);Compile(p);
    Check(gl->glIsEnabled(GL_SCISSOR_TEST),"clear restores scissor state");gl->glDisable(GL_SCISSOR_TEST);
    auto first=Draw(p,u,gl);Near(qRed(first.pixel(24,12)),32,"recompile resets history");Near(qGreen(first.pixel(24,12)),0,"recompile resets alpha");
    p.Resize(81,39);first=Draw(p,u,gl);Check(first.size()==QSize(81,39),"resize dimensions");Near(qRed(first.pixel(75,30)),32,"resize resets both surfaces");
    p.CleanupGL();p.Init();Compile(p);first=Draw(p,u,gl);Near(qRed(first.pixel(24,12)),32,"stop then restart resets history");
    p.main_pass->Resize(5000,5000);first=Draw(p,u,gl);Check(first.size()==QSize(81,39),"invalid resize cannot allocate unbounded feedback");
    p.main_pass->data.feedback=false;Compile(p);first=Draw(p,u,gl);Near(qRed(first.pixel(24,12)),32,"disabled history detached");
    first=Draw(p,u,gl);Near(qRed(first.pixel(24,12)),32,"ordinary pass does not accumulate");
    p.main_pass->data.feedback=true;first=Draw(p,u,gl);Near(qRed(first.pixel(24,12)),32,"enable starts empty history");p.CleanupGL();

    // A later shader pass must sample the newly completed history output,
    // not the alternate (one-frame-old) framebuffer.
    ShaderProgram chain;chain.SetVersion("130");chain.Resize(32,16);
    auto* buffer=new ShaderPass(ShaderPass::BUFFER);buffer->data.feedback=true;
    buffer->data.fragment_shader="void mainImage(out vec4 c,in vec2 p){c=texture2D(iPreviousFrame,p/iResolution.xy)+vec4(0.125,0,0,0);}";
    chain.passes.push_back(buffer);
    chain.main_pass->data.fragment_shader="void mainImage(out vec4 c,in vec2 p){c=texture2D(iChannel0,p/iResolution.xy);}";
    chain.Init();Compile(chain);
    for(int i=1;i<=3;++i) { auto image=Draw(chain,u,gl);Near(qRed(image.pixel(8,8)),32*i,"channel sees latest completed output"); }
    chain.CleanupGL();delete buffer;chain.passes.clear();
}
static std::string Prefix(const json& spec)
{
    std::string s="uniform vec4 iTapEvents[64];\nuniform vec4 iTapMeta[64];\nuniform float iTapCount;\n";
    for(const auto& c:spec.at("controls"))
    {
        std::string key=c.at("key"),type=c.at("type");s+="uniform "+std::string(type=="color"?"vec3":"float")+" p_"+key+";\n";
        if(type=="number")s+="uniform float t_"+key+";\n";
    }
    return s;
}
static Uniforms Params(const json& spec,float t,json overrides=json::object())
{
    Uniforms u;u.iTime=t;
    for(const auto& c:spec.at("controls"))
    {
        std::string key=c.at("key"),type=c.at("type");auto value=overrides.value(key,c.at("default"));
        if(type=="color") {QColor col(QString::fromStdString(value));u.custom["p_"+key]={{float(col.redF()),float(col.greenF()),float(col.blueF()),0},3};}
        else if(type=="boolean")u.custom["p_"+key].values[0]=value.get<bool>()?1.f:0.f;
        else {u.custom["p_"+key].values[0]=value.get<float>();u.custom["t_"+key].values[0]=value.get<float>()*t;}
    }
    return u;
}
static QImage Fresh(ShaderProgram& p,const Uniforms& u,QOpenGLFunctions* gl) {Compile(p);return Draw(p,u,gl);}
static int NeonTests(const QString& repo,const QString& output,QOpenGLFunctions* gl)
{
    const auto spec=Read(repo+"/Effects/SignalFavorites/presets/NeonNebula.json");
    Check(spec.at("controls").size()==8,"all eight controls");Check(spec.at("feedback")==true,"preset opts into history");
    Check(spec.at("tap_speed_key")=="speedRaw","tap uses correct speed integral");
    QFile shader(repo+"/shaders/SignalFavorites/NeonNebula.fs");Check(shader.open(QIODevice::ReadOnly),"shader file");
    ShaderProgram p;p.SetVersion("130");p.Resize(800,500);p.main_pass->data.feedback=true;
    p.main_pass->data.fragment_shader=Prefix(spec)+shader.readAll().toStdString();p.Init();Compile(p);
    auto start=Draw(p,Params(spec,0),gl);auto grown=start;QElapsedTimer timer;timer.start();
    for(int frame=1;frame<=60;++frame)grown=Draw(p,Params(spec,float(frame)/60.f),gl);
    const double ms=double(timer.nsecsElapsed())/1000000.0/60.0;
    Check(start!=grown,"autonomous animation and history evolve");
    Check(grown.save(output+"/NeonNebula-after-60.png"),"preview saved");
    // Same parameter snapshot rendered twice must overpaint prior strokes.
    auto u=Params(spec,1.3f);auto once=Fresh(p,u,gl);auto twice=Draw(p,u,gl);Check(once!=twice,"source-over accumulation retained");
    for(const auto& c:spec.at("controls"))
    {
        std::string key=c.at("key"),type=c.at("type");std::vector<json> values;
        if(type=="color")values={"#000000","#ffffff"};else if(type=="boolean")values={false,true};else values={c.at("min"),c.at("max")};
        for(const auto& value:values)Fresh(p,Params(spec,1.3f,{{key,value}}),gl);
    }
    // Force all ordinary layers black. Probe a top-left ring at known radius,
    // compare tap-off at the same pixel, then test persisted trail and expiry.
    json black={{"color1","#000000"},{"color2","#ff0000"},{"color3","#000000"},{"color4","#000000"},{"color5","#000000"},{"color6","#000000"}};
    u=Params(spec,0,black);u.custom["iTapCount"].values[0]=1;
    u.custom["iTapEvents[0]"]={{160,100,0.1f,1},4};u.custom["iTapMeta[0]"]={{0.5f,0.5f,0,0},4};
    auto tapped=Fresh(p,u,gl);const int x=565,y=250;
    Near(qRed(tapped.pixel(x,y)),255,"tap ring radius and coordinate convention");
    auto off=u;off.custom["p_tapEffect"].values[0]=0;const auto untapped=Fresh(p,off,gl);
    Check(tapped!=untapped,"keypress toggle changes actual ring");
    Check(qRed(untapped.pixel(x,y))<200,"known probe is not a background red stroke");
    Fresh(p,u,gl);u.custom["iTapCount"].values[0]=0;const auto trail=Draw(p,u,gl);
    Check(qRed(trail.pixel(x,y))>qRed(untapped.pixel(x,y)),"tap trail persists after active event ends");
    u.custom["iTapCount"].values[0]=1;u.custom["iTapMeta[0]"].values[1]=5.0f;
    Check(Fresh(p,u,gl)==untapped,"expired ring cannot draw");
    // Integrated speed is unchanged when the instantaneous speed changes.
    u.custom["iTapMeta[0]"].values[1]=0.5f;u.custom["p_speedRaw"].values[0]=1;
    auto slow=Fresh(p,u,gl);u.custom["p_speedRaw"].values[0]=10;auto fast=Fresh(p,u,gl);
    Check(slow==fast,"speed edit cannot jump an existing ring or phase integral");
    p.CleanupGL();
    std::ofstream((output+"/neon-validation.json").toStdString())<<json({{"checks",checks},{"width",800},{"height",500},{"frames",60},{"mean_draw_readback_ms",ms},{"timing_scope","CPU wall time of production draw plus synchronous FBO readback; not physical device FPS"}}).dump(2)<<'\n';
    return 0;
}
int main(int argc,char** argv)
{
    if(argc!=3)return 2;QGuiApplication app(argc,argv);int result=1;std::thread worker;
    QTimer::singleShot(0,&app,[&]{worker=std::thread([&]{try {
        QOffscreenSurface surface;surface.create();QOpenGLContext context;context.setFormat(surface.format());
        Check(context.create()&&context.makeCurrent(&surface),"GL context");QDir().mkpath(argv[2]);
        FeedbackTests(context.functions());result=NeonTests(argv[1],argv[2],context.functions());
        std::cout<<"PASS "<<checks<<" checks: production feedback lifecycle + Neon GPU\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';result=1;}
    QMetaObject::invokeMethod(&app,&QGuiApplication::quit,Qt::QueuedConnection);});});
    QTimer::singleShot(120000,&app,[]{std::_Exit(4);});app.exec();if(worker.joinable())worker.join();return result;
}
