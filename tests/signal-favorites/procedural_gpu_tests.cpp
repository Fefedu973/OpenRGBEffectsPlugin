// SPDX-License-Identifier: GPL-2.0-or-later
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QElapsedTimer>
#include "ShaderProgram.h"
#include "ProceduralEffectState.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <stdexcept>
static unsigned checks=0;
static void Check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
static json Read(const QString& path){std::ifstream in(path.toStdString());json j;in>>j;return j;}
static void Compile(ShaderProgram& p){auto log=p.Compile();if(!log.isEmpty()&&(!log.contains("warning C7533")||log.contains("error",Qt::CaseInsensitive)))throw std::runtime_error(log.toStdString());}
static QImage Draw(ShaderProgram& p,const Uniforms& u,QOpenGLFunctions* gl){p.Draw(u,gl);auto image=p.Image();Check(!image.isNull(),"image produced");Check(gl->glGetError()==GL_NO_ERROR,"GL error");return image;}
static json Defaults(const json& spec,const json& overrides=json::object()){json p=json::object();for(const auto& c:spec.at("controls"))p[c.at("key").get<std::string>()]=c.at("default");p.update(overrides);return p;}
static Uniforms Params(const json& spec,const json& params,native_procedural::State& state,double dt)
{
    Uniforms u;
    for(const auto& c:spec.at("controls"))
    {
        std::string key=c.at("key"),type=c.at("type");auto v=params.at(key);
        if(type=="color"){QColor color(QString::fromStdString(v));u.custom["p_"+key]={{float(color.redF()),float(color.greenF()),float(color.blueF()),0},3};}
        else if(type=="boolean")u.custom["p_"+key].values[0]=v.get<bool>()?1.f:0.f;
        else if(type=="enum"){const auto& options=c.at("options");u.custom["p_"+key].values[0]=float(std::find(options.begin(),options.end(),v)-options.begin());}
        else u.custom["p_"+key].values[0]=v.get<float>();
    }
    auto extra=state.Update(spec.at("id"),params,dt);u.custom.insert(extra.begin(),extra.end());return u;
}
static QRgb Pixel(const QImage& image,float x,float y){return image.pixel(int(x*image.width()/320),int(y*image.height()/200));}
static void RGB(const QImage& image,float x,float y,int r,int g,int b,const char* why){auto p=Pixel(image,x,y);Check(std::abs(qRed(p)-r)<=3&&std::abs(qGreen(p)-g)<=3&&std::abs(qBlue(p)-b)<=3,why);}
static void StateTests()
{
    native_procedural::State state;
    auto V=[&](const char* id,json p,double dt){return state.Update(id,p,dt).at("prState").values;};
    auto v=V("Visor",{{"speed",30}},0);Check(v[0]==320,"Visor starts beyond right edge");v=V("Visor",{{"speed",30}},1.0/60);Check(v[0]==323,"Visor high-edge overshoot retained");v=V("Visor",{{"speed",30}},1.0/60);Check(v[0]==320,"Visor reverses on next tick");
    v=V("Visor",{{"speed",30},{"vertical",false}},1.0/60);Check(v[0]==200,"horizontal motion retains separate initial position");
    state.Reset();v=V("CustomWave",{{"speed",50}},0);Check(v[0]==2.5,"wave advances before first draw");
    for(int i=0;i<120;++i)v=V("CustomWave",{{"speed",50}},1.0/60);Check(v[0]==302.5,"wave draws overshoot before wrapping");v=V("CustomWave",{{"speed",50}},1.0/60);Check(v[0]==2.5,"wave starts again after overshoot");
    state.Reset();v=V("Pinwheel",{{"bounce",true},{"bounceSpeed",40}},0);Check(v[0]==1&&v[1]==0&&v[2]==0,"pinwheel first frame original origin/angle");v=V("Pinwheel",{{"bounce",true},{"bounceSpeed",40}},1.0/60);Check(v[0]==1.5f&&v[1]==4&&v[2]==0,"pinwheel postdraw increments");
    state.Reset();V("Pinwheel",{{"bounce",true},{"edgeToEdge","Counter Clockwise"}},0);v=V("Pinwheel",{{"bounce",true},{"edgeToEdge","Counter Clockwise"}},1.0/60);Check(v[1]==0&&v[2]==4,"counterclockwise first corner uses two independent conditions");
    state.Reset();v=V("Plasma",{{"speed",20}},0);Check(std::abs(v[0]-.004)<1e-6&&v[1]==5&&v[2]==0,"plasma noise advances but background uses previous time");
    for(int i=0;i<77;++i)v=V("Plasma",{{"speed",0}},1.0/60);Check(v[1]==0,"gradient wraps after385 not at385");
    native_procedural::State a,b;json params={{"speed",53},{"nColors",3},{"bVertical",true}};a.Update("CustomWave",params,0);b.Update("CustomWave",params,0);
    for(int i=0;i<600;++i)a.Update("CustomWave",params,1.0/60);for(int i=0;i<300;++i)b.Update("CustomWave",params,1.0/30);
    Check(a.Update("CustomWave",params,0).at("prState").values==b.Update("CustomWave",params,0).at("prState").values,"60Hz numeric animation independent from render cadence");
    auto frozen=a.Update("CustomWave",params,0).at("prState").values;
    Check(a.Update("CustomWave",params,std::nan("")).at("prState").values==frozen,"invalid delta cannot poison state");
    Check(a.Update("Unknown",params,1000).empty(),"unknown effect leaves shared helper unused");
}
static void SpinReference(const QImage& actual,float gap,float width,float angle,const QString& output)
{
    QImage reference(actual.size(),QImage::Format_RGB32);reference.fill(Qt::black);QPainter painter(&reference);painter.setRenderHint(QPainter::Antialiasing);painter.scale(actual.width()/320.0,actual.height()/200.0);
    QPainterPath path;path.moveTo(160,100);const double pi=3.141592653589793;for(int n=1;n<=599;++n){double t=n*2*pi/60;path.lineTo(160+t*std::cos(t+angle)*gap,100+t*std::sin(t+angle)*gap);}
    QPen pen(Qt::white);pen.setWidthF(width);pen.setCapStyle(Qt::FlatCap);pen.setJoinStyle(Qt::MiterJoin);painter.setPen(pen);painter.drawPath(path);painter.end();
    unsigned valid=0,mismatch=0;
    // Compare stable interiors only: Qt and Canvas/GPU edge coverage differ.
    for(int y=2;y<actual.height()-2;y+=3)for(int x=2;x<actual.width()-2;x+=3)
    {
        int v=qRed(reference.pixel(x,y));if(v!=0&&v!=255)continue;bool stable=true;
        for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)if(qRed(reference.pixel(x+dx,y+dy))!=v)stable=false;
        if(!stable)continue;++valid;if(std::abs(qRed(actual.pixel(x,y))-v)>4){if(mismatch<5)std::cerr<<"Mismatch "<<x<<","<<y<<" expected "<<v<<" got "<<qRed(actual.pixel(x,y))<<" gap "<<gap<<" width "<<width<<" angle "<<angle<<"\n";++mismatch;}
    }
    Check(valid>1000,"independent spiral reference contains enough stable pixels");if(mismatch){reference.save(output+"/spin-reference-fail.png");actual.save(output+"/spin-actual-fail.png");std::cerr<<"mismatches="<<mismatch<<" stable="<<valid<<"\n";}Check(mismatch==0,"exact599segment/miter geometry matches independent Qt rasterizer interiors");
}
static void Run(const QString& repo,const QString& output,QOpenGLFunctions* gl)
{
    StateTests();json timings=json::object();unsigned boundaries=0,controls=0;
    for(const QString slug:{"visor","custom-wave","pinwheel","spin","plasma"})
    {
        auto spec=Read(repo+"/Effects/SignalFavorites/presets/"+slug+".json");controls+=spec.at("controls").size();std::string prefix;
        for(const auto& c:spec.at("controls")){std::string type=c.at("type"),key=c.at("key");prefix+="uniform "+std::string(type=="color"?"vec3":"float")+" p_"+key+";\n";}
        QFile shader(repo+"/shaders/SignalFavorites/"+slug+".fs");Check(shader.open(QIODevice::ReadOnly),"individual shader source");ShaderProgram p;p.SetVersion("130");p.Resize(800,500);p.main_pass->data.feedback=spec.value("feedback",false);p.main_pass->data.fragment_shader=prefix+shader.readAll().toStdString();p.Init();Compile(p);
        native_procedural::State state;auto defaults=Defaults(spec);auto u=Params(spec,defaults,state,0);auto first=Draw(p,u,gl);QImage animated;
        for(int frame=0;frame<90;++frame)animated=Draw(p,Params(spec,defaults,state,1.0/60),gl);
        Check(first!=animated,"default effect visibly animates");Check(animated.save(output+"/"+slug+".png"),"preview saved");
        if(slug=="visor")
        {
            Compile(p);state.Reset();auto params=Defaults(spec,{{"trail",50},{"color","#ff0000"},{"bgColor","#000000"}});u=Params(spec,params,state,0);u.custom["prState"].values[0]=50;u.custom["prColor"]={{1,0,0,0},3};auto red=Draw(p,u,gl);RGB(red,55,100,255,0,0,"vertical bar geometry");u.custom["prState"].values[0]=100;auto faded=Draw(p,u,gl);RGB(faded,55,100,128,0,0,"trail blends previous pixel once");Compile(p);RGB(Draw(p,u,gl),55,100,0,0,0,"recompile clears history");
        }
        if(slug=="custom-wave")
        {
            for(int n=2;n<=4;++n)for(bool vertical:{false,true})
            {
                state.Reset();auto params=Defaults(spec,{{"speed",0},{"nColors",n},{"bVertical",vertical}});auto image=Draw(p,Params(spec,params,state,0),gl);
                // Test every50-unit band against the independently enumerated
                // source rectangle ordering, including repeated colors.
                int palette[4][3]={{255,0,0},{0,0,255},{0,255,0},{255,255,0}};
                for(int band=0;band<(vertical?4:6);++band){int k=n==2?(band%2==0?1:0):n==3?(2-band%3):band%4;RGB(image,vertical?10:band*50+20,vertical?band*50+20:10,palette[k][0],palette[k][1],palette[k][2],"wave palette ordering/orientation");}
            }
        }
        if(slug=="pinwheel")
        {
            state.Reset();auto params=Defaults(spec,{{"background","#000000"},{"pinwheelColor","#ffffff"},{"speed",0}});auto image=Draw(p,Params(spec,params,state,0),gl);RGB(image,160,100,255,255,255,"pinwheel radius5 hub");
            params["bounce"]=true;state.Reset();image=Draw(p,Params(spec,params,state,0),gl);RGB(image,1,1,255,255,255,"edge mode starts at upper left");
        }
        if(slug=="spin")
        {
            for(float gap:{1.f,4.f,10.f})for(float width:{1.f,5.f,20.f})for(float angle:{0.f,1.234f,-2.17f})
            {
                state.Reset();auto params=Defaults(spec,{{"rainbow",false},{"fg","#ffffff"},{"bg","#000000"},{"lineGap",gap},{"size",width},{"speed",50}});u=Params(spec,params,state,0);u.custom["prState"].values[0]=angle*100;SpinReference(Draw(p,u,gl),gap,width,angle,output);
            }
            state.Reset();auto params=Defaults(spec,{{"speed",0}});auto a=Draw(p,Params(spec,params,state,0),gl),b=Draw(p,Params(spec,params,state,.25),gl);Check(a==b,"zero speed freezes spin hue and rotation");
        }
        if(slug=="plasma")
        {
            state.Reset();auto params=Defaults(spec,{{"shape","Fill All"},{"colorMode","Color Cycle"},{"colorCycleSpeed",0},{"gap",5},{"dotSize",6},{"bgColor","#0000ff"}});auto image=Draw(p,Params(spec,params,state,0),gl);RGB(image,4,4,255,0,0,"plasma filled cell");RGB(image,13,4,0,0,255,"plasma horizontal gap");RGB(image,4,13,0,0,255,"plasma vertical gap");
            params["colorMode"]="Default";params["hueRange"]=0;params["pixelColor"]="#00ff00";image=Draw(p,Params(spec,params,state,0),gl);RGB(image,4,4,0,255,0,"plasma baseHSL roundtrip");
            params["bgcolorMode"]="Color Cycle";params["colorMode"]="Color Cycle";u=Params(spec,params,state,0);u.custom["prState"].values[2]=0;image=Draw(p,u,gl);RGB(image,13,4,0,255,170,"background cycle uses independent old noise time and160degree offset");
            params=Defaults(spec,{{"shape","Fill"},{"hueRange",0},{"pixelColor","#ff0000"},{"bgColor","#000000"}});u=Params(spec,params,state,0);u.custom["prState"].values[0]=.42f;auto fill=Draw(p,u,gl);u.custom["p_shape"].values[0]=2;auto lines=Draw(p,u,gl);u.custom["p_shape"].values[0]=1;auto all=Draw(p,u,gl);unsigned partial=0,opaque=0;
            for(int y=3;y<200;y+=11)for(int x=3;x<320;x+=11)
            {
                int alpha=qRed(Pixel(fill,float(x),float(y)));RGB(all,float(x),float(y),255,0,0,"FillAll ignores the noise alpha mask");
                if(alpha>3&&alpha<251){RGB(lines,float(x),float(y),255,0,0,"Line keeps only intermediate positive noise");++partial;}
                if(alpha==255){RGB(lines,float(x),float(y),0,0,0,"Line removes saturated positive noise");++opaque;}
            }
            Check(partial>5&&opaque>5,"noise field exercises partial and saturated alpha ranges");
            params["shape"]="Fill All";params["colorMode"]="Gradient Wave";params["gap"]=0;u=Params(spec,params,state,0);u.custom["prState"].values[1]=0;image=Draw(p,u,gl);RGB(image,64,4,87,0,163,"gradient has repeated violet center stop");
        }
        for(const auto& c:spec.at("controls"))
        {
            const std::string type=c.at("type"),key=c.at("key");std::vector<json> values;
            if(type=="number")values={c.at("min"),c.at("max")};else if(type=="boolean")values={false,true};else if(type=="color")values={"#000000","#ffffff"};else for(const auto& v:c.at("options"))values.push_back(v);
            for(const auto& value:values){state.Reset();Compile(p);auto params=Defaults(spec,{{key,value}});Draw(p,Params(spec,params,state,.25),gl);++boundaries;}
        }
        Compile(p);state.Reset();QElapsedTimer timer;timer.start();for(int i=0;i<20;++i)Draw(p,Params(spec,defaults,state,1.0/60),gl);timings[slug.toStdString()]=double(timer.nsecsElapsed())/1e6/20;p.CleanupGL();
    }
    Check(controls==43,"43 original controls");std::ofstream((output+"/procedural-validation.json").toStdString())<<json({{"checks",checks},{"control_boundary_cases",boundaries},{"width",800},{"height",500},{"mean_draw_readback_ms",timings},{"scope","production GPU rendering plus synchronous readback; no hardware and no pixel-perfect browser claim"}}).dump(2)<<'\n';std::cout<<"PASS "<<checks<<" GPU/state checks, "<<boundaries<<" control boundaries\n";
}
int main(int argc,char** argv)
{
    if(argc!=3)return 2;QGuiApplication app(argc,argv);int result=1;std::thread worker;QTimer::singleShot(0,&app,[&]{worker=std::thread([&]{try{QOffscreenSurface surface;surface.create();QOpenGLContext context;context.setFormat(surface.format());Check(context.create()&&context.makeCurrent(&surface),"GL context");QDir().mkpath(argv[2]);Run(argv[1],argv[2],context.functions());result=0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';}QMetaObject::invokeMethod(&app,&QGuiApplication::quit,Qt::QueuedConnection);});});QTimer::singleShot(180000,&app,[]{std::_Exit(4);});app.exec();if(worker.joinable())worker.join();return result;
}
