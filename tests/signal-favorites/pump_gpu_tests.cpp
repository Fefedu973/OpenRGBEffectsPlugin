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
static unsigned checks=0;
static void Check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
static json Read(const QString& path){std::ifstream in(path.toStdString());json j;in>>j;return j;}
static void Compile(ShaderProgram& p)
{auto log=p.Compile();if(!log.isEmpty()&&(!log.contains("warning C7533")||log.contains("error",Qt::CaseInsensitive)))throw std::runtime_error(log.toStdString());}
static QImage Draw(ShaderProgram& p,const Uniforms& u,QOpenGLFunctions* gl)
{p.Draw(u,gl);auto image=p.Image();Check(!image.isNull(),"image produced");Check(gl->glGetError()==GL_NO_ERROR,"GL error");return image;}
static Uniforms Params(const json& spec,const json& overrides=json::object())
{
    Uniforms u;
    for(const auto& c:spec.at("controls"))
    {
        std::string key=c.at("key"),type=c.at("type");auto v=overrides.value(key,c.at("default"));
        if(type=="color"){QColor color(QString::fromStdString(v));u.custom["p_"+key]={{float(color.redF()),float(color.greenF()),float(color.blueF()),0},3};}
        else if(type=="boolean")u.custom["p_"+key].values[0]=v.get<bool>()?1.f:0.f;
        else if(type=="enum"){const auto& options=c.at("options");u.custom["p_"+key].values[0]=float(std::find(options.begin(),options.end(),v)-options.begin());}
        else u.custom["p_"+key].values[0]=v.get<float>();
    }
    u.custom["pumpLevels"]={{.5f,.25f,.5f,.75f},4};u.custom["pumpState"]={{.15f,.6f,0.f,0.f},4};
    for(unsigned i=0;i<100;++i)u.custom["pumpFreq["+std::to_string(i)+"]"].values[0]=.5f;
    return u;
}
static QRgb Pixel(const QImage& image,float x,float y){return image.pixel(int(x*image.width()/320),int(y*image.height()/200));}
static void RGB(const QImage& image,float x,float y,int r,int g,int b,const char* why)
{auto p=Pixel(image,x,y);Check(std::abs(qRed(p)-r)<=3&&std::abs(qGreen(p)-g)<=3&&std::abs(qBlue(p)-b)<=3,why);}
static void Run(const QString& repo,const QString& output,QOpenGLFunctions* gl)
{
    const auto spec=Read(repo+"/Effects/SignalFavorites/presets/pump-up-beats.json");
    Check(spec.at("controls").size()==20,"twenty controls retained");Check(spec.at("feedback")==true&&spec.at("audioReactive")==true,"audio/feedback opt-ins");
    std::string prefix;
    for(const auto& c:spec.at("controls")){std::string type=c.at("type"),key=c.at("key");prefix+="uniform "+std::string(type=="color"?"vec3":"float")+" p_"+key+";\n";}
    QFile shader(repo+"/shaders/SignalFavorites/pump-up-beats.fs");Check(shader.open(QIODevice::ReadOnly),"shader file");
    ShaderProgram p;p.SetVersion("130");p.Resize(800,500);p.main_pass->data.feedback=true;p.main_pass->data.fragment_shader=prefix+shader.readAll().toStdString();p.Init();Compile(p);
    const json palette={{"colorStyle","Static"},{"staticCol1","#ff0000"},{"staticCol2","#0000ff"}};
    auto u=Params(spec,palette);auto image=Draw(p,u,gl);
    RGB(image,10,150,255,0,0,"left VU first static half");RGB(image,40,150,0,0,255,"left VU second static half");
    RGB(image,10,50,0,0,0,"left VU is bottom aligned");
    RGB(image,60,10,255,0,0,"bass bar upper static half");RGB(image,60,20,0,0,255,"bass bar lower static half");
    RGB(image,150,10,0,0,0,"bass bar length independent of volume");
    RGB(image,190,10,191,0,0,"bass rectangle gain");RGB(image,230,10,64,0,0,"volume rectangle separate gain");
    RGB(image,295,4,255,0,0,"wraith top is full brightness");RGB(image,295,15,191,0,0,"wraith middle tracks bass");
    RGB(image,250,113,0,0,255,"central right spectrum static color2");RGB(image,110,113,255,0,0,"central left spectrum static color1");
    RGB(image,250,198,0,0,255,"bottom frequency line gain");
    Check(image.save(output+"/PumpUpBeats-static.png"),"static preview saved");
    // Keep frame history only in non-reset areas; fading never makes the
    // separate top brightness rectangles or bottom spectrum retain old audio.
    u.custom["p_fadingOut"].values[0]=50;Draw(p,u,gl);
    u.custom["pumpLevels"]={{0,0,0,0},4};for(unsigned i=0;i<100;++i)u.custom["pumpFreq["+std::to_string(i)+"]"].values[0]=0;
    auto faded=Draw(p,u,gl);Check(qBlue(Pixel(faded,250,113))>245,"central history decays gradually");
    RGB(faded,250,198,0,0,0,"bottom history is erased each frame");RGB(faded,190,10,0,0,0,"bass rectangle does not trail");
    Compile(p);auto reset=Draw(p,u,gl);RGB(reset,250,113,0,0,0,"recompile clears trail");
    u=Params(spec,{{"colorStyle","HueWave"}});Compile(p);image=Draw(p,u,gl);
    RGB(image,250,198,0,0,0,"reference wave bottom block composes brightness squared");
    std::vector<QImage> styles;
    for(const auto& style:json::array({"Bars","ThinBars","ChonkerBars","Smooth","SmoothSimple","Pixel"}))
    {
        u=Params(spec,{{"displayStyle",style},{"colorStyle","HueWave"}});
        for(unsigned i=0;i<100;++i)u.custom["pumpFreq["+std::to_string(i)+"]"].values[0]=float(.1+.7*std::abs(std::sin(i*.52)));
        Compile(p);styles.push_back(Draw(p,u,gl));
    }
    for(unsigned i=0;i<styles.size();++i)for(unsigned j=i+1;j<styles.size();++j)Check(styles[i]!=styles[j],"six spectrum modes have distinct geometry");
    Check(styles[0].save(output+"/PumpUpBeats-wave.png"),"wave preview saved");
    unsigned boundaries=0;
    for(const auto& c:spec.at("controls"))
    {
        const std::string type=c.at("type"),key=c.at("key");std::vector<json> values;
        if(type=="number")values={c.at("min"),c.at("max")};else if(type=="boolean")values={false,true};
        else if(type=="color")values={"#000000","#ffffff"};else for(const auto& v:c.at("options"))values.push_back(v);
        for(const auto& v:values){Compile(p);Draw(p,Params(spec,{{key,v}}),gl);++boundaries;}
    }
    // Worst permitted border combination and independent volume/bass extremes.
    for(float size:{120.f,199.f})for(float line:{0.f,50.f})
    {Compile(p);u=Params(spec,{{"freqDisplaySize",size},{"bottomLineThickness",line},{"usedFreqSector",100}});Draw(p,u,gl);}
    Compile(p);u=Params(spec);QElapsedTimer timer;timer.start();for(int i=0;i<60;++i)Draw(p,u,gl);
    double ms=double(timer.nsecsElapsed())/1e6/60;
    std::ofstream((output+"/pump-validation.json").toStdString())<<json({{"checks",checks},{"control_boundary_cases",boundaries},{"width",800},{"height",500},{"mean_draw_readback_ms",ms},{"timing_scope","production GPU draw plus synchronous CPU readback, not hardware refresh rate"}}).dump(2)<<'\n';
    p.CleanupGL();std::cout<<"PASS "<<checks<<" GPU checks, "<<boundaries<<" control boundaries, "<<ms<<"ms draw/readback\n";
}
int main(int argc,char** argv)
{
    if(argc!=3)return 2;QGuiApplication app(argc,argv);int result=1;std::thread worker;
    QTimer::singleShot(0,&app,[&]{worker=std::thread([&]{try{QOffscreenSurface surface;surface.create();QOpenGLContext context;context.setFormat(surface.format());Check(context.create()&&context.makeCurrent(&surface),"GL context");QDir().mkpath(argv[2]);Run(argv[1],argv[2],context.functions());result=0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';}QMetaObject::invokeMethod(&app,&QGuiApplication::quit,Qt::QueuedConnection);});});
    QTimer::singleShot(120000,&app,[]{std::_Exit(4);});app.exec();if(worker.joinable())worker.join();return result;
}
