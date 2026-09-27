// SPDX-License-Identifier: GPL-2.0-or-later
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QTimer>
#include <QPainter>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QColor>
#include <QElapsedTimer>
#include "ShaderProgram.h"
#include "BasicEffectState.h"
#include "PumpDynamics.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <numeric>
static void Check(bool value,const char* reason) { if(!value) throw std::runtime_error(reason); }
static json Read(const QString& path) { std::ifstream in(path.toStdString()); json j; in>>j; return j; }
static Uniforms Parameters(const json& spec,const json& values,float time)
{
    Uniforms uniforms; uniforms.iTime=time; uniforms.custom["iTap"]={{160,100,0.5f,1},4};
    uniforms.custom["iTapCount"].values[0]=1;
    uniforms.custom["iTapEvents[0]"]={{160,100,0.5f,1},4};
    uniforms.custom["iTapMeta[0]"]={{.25f,5.0f+std::fmod(time,1.0f)*10.0f,0,0},4};
    json basic_parameters=json::object();
    for(const auto& c:spec.at("controls"))
    {
        const std::string key=c.at("key"),type=c.at("type");
        auto& p=uniforms.custom["p_"+key]; const auto value=values.value(key,c.at("default"));
        basic_parameters[key]=value;
        if(type=="color") {const QColor col(QString::fromStdString(value)); p={{float(col.redF()),float(col.greenF()),float(col.blueF()),0},3};}
        else if(type=="boolean") p.values[0]=value.get<bool>()?1.f:0.f;
        else if(type=="enum") {const auto& opts=c.at("options");p.values[0]=float(std::distance(opts.begin(),std::find(opts.begin(),opts.end(),value)));}
        else {p.values[0]=value.get<float>();uniforms.custom["t_"+key].values[0]=time*p.values[0];}
    }
    native_basic::State state; ShaderUniformMap basic;
    const unsigned ticks=unsigned(std::clamp(time*60,1.f,600.f));
    for(unsigned i=0;i<ticks;++i)basic=state.Update(spec.at("id"),basic_parameters,1.0/60);
    uniforms.custom.insert(basic.begin(),basic.end());
    if(spec.at("id")=="PumpUpBeats")
    {
        native_pump::State pump;native_pump::Frame frame;
        room_audio::RhythmSnapshot audio;audio.generation=1;audio.power=.3f;audio.silent=false;
        for(unsigned i=0;i<audio.spectrum.size();++i)audio.spectrum[i]=.4f*std::exp(-float(i)/30.f);
        for(unsigned i=0;i<ticks;++i)frame=pump.Update(basic_parameters,1.0/60,audio);
        uniforms.custom["pumpLevels"]={frame.levels,4};
        uniforms.custom["pumpState"]={frame.state,4};
        for(unsigned i=0;i<frame.frequencies.size();++i)
            uniforms.custom["pumpFreq["+std::to_string(i)+"]"].values[0]=frame.frequencies[i];
    }
    return uniforms;
}
// Independent analytic checkpoints. Qt's FBO image is top-left oriented, as is
// canvas_point. Pixel centers may be up to 0.2 reference pixels from the target.
static json VerifyColorPoints(ShaderProgram& program,const json& spec,
                              const QString& slug,const json& cases,
                              QOpenGLFunctions* gl)
{
    json results=json::array();
    for(const auto& entry:cases)
    {
        if(entry.at("preset").get<std::string>()!=slug.toStdString()) continue;
        auto uniforms=Parameters(spec,entry.value("parameters",json::object()),0);
        const auto integrals=entry.value("integrals",json::object());
        for(auto it=integrals.begin();it!=integrals.end();++it)
            uniforms.custom["t_"+it.key()].values[0]=it.value().get<float>();
        program.Draw(uniforms,gl);
        const auto image=program.Image();
        Check(!image.isNull(),"color checkpoint has no image");
        Check(gl->glGetError()==GL_NO_ERROR,"GL error at color checkpoint");
        const auto& point=entry.at("canvas_point");
        const int x=std::clamp(int(point[0].get<double>()*image.width()/320.0),0,image.width()-1);
        const int y=std::clamp(int(point[1].get<double>()*image.height()/200.0),0,image.height()-1);
        const QColor actual=image.pixelColor(x,y);
        const double channels[3]={actual.redF(),actual.greenF(),actual.blueF()};
        double maximum_error=0;
        for(int c=0;c<3;++c)
            maximum_error=std::max(maximum_error,std::abs(channels[c]-entry.at("rgb")[c].get<double>()));
        const bool passed=maximum_error<=entry.at("tolerance").get<double>();
        results.push_back({{"canvas_point",point},{"pixel",{x,y}},
                           {"actual_rgb",{channels[0],channels[1],channels[2]}},
                           {"expected_rgb",entry.at("rgb")},{"maximum_error",maximum_error},
                           {"passed",passed}});
        if(!passed)
            throw std::runtime_error(slug.toStdString()+" color checkpoint failed: "+results.back().dump());
    }
    return results;
}
static json MeasureRenderReadback(ShaderProgram& program,const json& spec,QOpenGLFunctions* gl)
{
    // Six frames after shader/default/variant warm-up. CPU-side Draw + FBO
    // readback latency, not a GPU-only timer or a hardware-device frame rate.
    std::vector<double> samples;
    for(int i=0;i<6;++i)
    {
        const auto uniforms=Parameters(spec,json::object(),float(i)*0.03125f+5.f);
        QElapsedTimer timer;timer.start();
        program.Draw(uniforms,gl);const auto image=program.Image();
        const double elapsed=double(timer.nsecsElapsed())/1000000.0;
        Check(!image.isNull(),"timed frame missing");
        Check(gl->glGetError()==GL_NO_ERROR,"GL error on timed frame");
        samples.push_back(elapsed);
    }
    const double average=std::accumulate(samples.begin(),samples.end(),0.0)/samples.size();
    std::sort(samples.begin(),samples.end());
    return {{"method","warm Draw plus synchronous FBO image readback; CPU wall time"},
            {"samples",samples.size()},{"average_ms",average},
            {"median_ms",(samples[2]+samples[3])*0.5},{"maximum_ms",samples.back()}};
}
static unsigned VerifyTapRings(ShaderProgram& program,const json& spec,const json& cases,QOpenGLFunctions* gl)
{
    unsigned count=0;
    for(const auto& entry:cases.at("cases"))
    {
        auto parameters=cases.at("base_parameters");
        if(entry.contains("parameters"))parameters.update(entry.at("parameters"));
        auto uniforms=Parameters(spec,parameters,0);
        uniforms.custom["iTapCount"].values[0]=float(entry.at("events").size());
        for(std::size_t i=0;i<entry.at("events").size();++i)
        {
            auto& event=uniforms.custom["iTapEvents["+std::to_string(i)+"]"];event.components=4;
            auto& meta=uniforms.custom["iTapMeta["+std::to_string(i)+"]"];meta.components=4;
            for(unsigned c=0;c<4;++c){event.values[c]=entry["events"][i][c];meta.values[c]=entry["meta"][i][c];}
        }
        program.Draw(uniforms,gl);const auto image=program.Image();
        Check(!image.isNull()&&gl->glGetError()==GL_NO_ERROR,"tap render failed");
        for(const auto& point:entry.at("points"))
        {
            const auto rgb=image.pixelColor(int(point.at("x").get<double>()*image.width()/320),
                                            int(point.at("y").get<double>()*image.height()/200));
            const int actual[]={rgb.red(),rgb.green(),rgb.blue()};
            for(unsigned c=0;c<3;++c)Check(std::abs(actual[c]-point["rgb"][c].get<int>())<=3,"tap color checkpoint failed");
            ++count;
        }
    }
    return count;
}
static int Render(const QString& repo,const QString& output)
{
    try {
        QOffscreenSurface surface; surface.create(); QOpenGLContext context;
        context.setFormat(surface.format()); Check(context.create()&&context.makeCurrent(&surface),"GL context unavailable");
        QDir().mkpath(output);
        const QDir presets(repo+"/Effects/SignalFavorites/presets");
        const auto files=presets.entryList({"*.json"},QDir::Files,QDir::Name);
        const auto color_cases=Read(repo+"/tests/signal-favorites/rainbow-family.cases.json").at("cases");
        Check(files.size()>=12,"expected at least twelve favorites");
        QImage sheet(960,((files.size()+2)/3)*222,QImage::Format_RGB32); sheet.fill(Qt::black); QPainter painter(&sheet);
        json evidence=json::array(); int index=0;
        for(const auto& file:files) {
            const auto spec=Read(presets.filePath(file));
            std::string prefix="uniform vec4 iTap;\nuniform float iTapCount;\nuniform vec4 iTapEvents[64];\nuniform vec4 iTapMeta[64];\n";
            prefix+=native_basic::State::Declarations(spec.at("id"));
            for(const auto& c:spec.at("controls")) {
                const std::string key=c.at("key"),type=c.at("type");
                prefix+="uniform "+std::string(type=="color"?"vec3":"float")+" p_"+key+";\n";
                if(type=="number") prefix+="uniform float t_"+key+";\n";
            }
            QFile shader(repo+"/shaders/SignalFavorites/"+QFileInfo(file).completeBaseName()+".fs");
            Check(shader.open(QIODevice::ReadOnly),"missing shader");
            ShaderProgram program; program.SetVersion("130"); program.Resize(800,500);
            program.main_pass->data.feedback=spec.value("feedback",false);
            program.main_pass->data.fragment_shader=prefix+shader.readAll().toStdString();
            program.Init(); const auto log=program.Compile(); if(!log.isEmpty()) throw std::runtime_error(file.toStdString()+": "+log.toStdString());
            program.Draw(Parameters(spec,json::object(),1),context.functions()); const auto first=program.Image();
            program.Draw(Parameters(spec,json::object(),4.125f),context.functions()); const auto second=program.Image();
            Check(second.width()==800&&second.height()==500,"wrong canvas size");
            unsigned lit=0,changed=0;
            for(int y=0;y<500;y+=4) for(int x=0;x<800;x+=4) {
                const auto c=second.pixel(x,y); if(qRed(c)+qGreen(c)+qBlue(c)>12) ++lit;
                if(first.pixel(x,y)!=c) ++changed;
            }
            Check(spec.at("id")=="GoodNight"?lit==0:lit>10,"unexpected blank/lit default render");
            Check(context.functions()->glGetError()==GL_NO_ERROR,"GL error on default render");
            json controls=json::array();
            for(const auto& c:spec.at("controls")) {
                const std::string key=c.at("key"),type=c.at("type");
                std::vector<json> cases;
                if(type=="number") cases={c.at("min"),c.at("max")};
                else if(type=="boolean") cases={false,true};
                else if(type=="enum") for(const auto& option:c.at("options"))cases.push_back(option);
                else cases={"#000000","#ffffff"};
                int differences=0;
                for(const auto& value:cases) {
                    program.Draw(Parameters(spec,{{key,value}},4.125f),context.functions());
                    const auto variant=program.Image(); Check(!variant.isNull(),"control variant frame missing");
                    Check(context.functions()->glGetError()==GL_NO_ERROR,"GL error on control boundary");
                    if(variant!=second)++differences;
                }
                controls.push_back({{"key",key},{"boundary_cases",cases.size()},{"different_frames",differences}});
            }
            const auto color_points=VerifyColorPoints(program,spec,QFileInfo(file).completeBaseName(),color_cases,context.functions());
            const auto tap_points=spec.at("id")=="RainbowTap"?
                VerifyTapRings(program,spec,Read(repo+"/tests/signal-favorites/rainbow-tap.cases.json"),context.functions()):0;
            const auto timing=MeasureRenderReadback(program,spec,context.functions());
            evidence.push_back({{"id",spec.at("id")},{"lit_samples",lit},{"animated_samples",changed},{"controls",controls},
                                {"color_checkpoints",color_points},{"tap_color_checkpoints",tap_points},{"render_readback",timing}});
            second.save(output+"/"+QFileInfo(file).completeBaseName()+".png");
            painter.drawImage(QRect((index%3)*320,(index/3)*222,320,200),second);
            painter.setPen(Qt::white); painter.drawText((index%3)*320+8,(index/3)*222+216,QString::fromStdString(spec.at("title")));
            ++index; program.CleanupGL();
        }
        painter.end(); Check(sheet.save(output+"/contact-sheet.png"),"image save failed");
        std::ofstream(QDir(output).filePath("gpu-validation.json").toStdString())<<evidence.dump(2)<<'\n';
        size_t verified=0;
        for(const auto& record:evidence) verified+=record.at("color_checkpoints").size();
        Check(verified==color_cases.size(),"not all analytic checkpoints were rendered");
        std::cout<<"PASS "<<files.size()<<" favorites: production GLSL renderer, 800x500, control boundaries\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
int main(int argc,char** argv)
{
    if(argc!=3) return 2; QGuiApplication app(argc,argv); int result=3; std::thread worker;
    QTimer::singleShot(0,&app,[&]{worker=std::thread([&]{result=Render(argv[1],argv[2]);QMetaObject::invokeMethod(&app,&QGuiApplication::quit,Qt::QueuedConnection);});});
    QTimer::singleShot(120000,&app,[]{std::_Exit(4);}); app.exec(); if(worker.joinable())worker.join(); return result;
}
