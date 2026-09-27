// SPDX-License-Identifier: GPL-2.0-or-later
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QTimer>
#include <QPainter>
#include <QDir>
#include "ShaderProgram.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <stdexcept>
static void Check(bool value,const char* reason) { if(!value) throw std::runtime_error(reason); }
static int Render(const QString& profiles,const QString& output)
{
    try {
        QOffscreenSurface surface; surface.create(); QOpenGLContext context;
        context.setFormat(surface.format()); Check(context.create()&&context.makeCurrent(&surface),"GL context unavailable");
        QImage sheet(800,810,QImage::Format_RGB32); sheet.fill(Qt::black); QPainter painter(&sheet);
        json evidence=json::array(); int index=0;
        const auto files=QDir(profiles).entryList({"Effet - *.json"},QDir::Files,QDir::Name);
        Check(files.size()==6,"expected six profiles");
        for(const auto& file:files) {
            std::ifstream input(QDir(profiles).filePath(file).toStdString()); json data; input>>data;
            auto* program=ShaderProgram::FromJSON(data["plugins"]["OpenRGB Effects Plugin"]["Effects"][0]["CustomSettings"]["shader_program"]);
            program->Init(); const auto errors=program->Compile();
            if(!errors.isEmpty()) throw std::runtime_error(errors.toStdString());
            Uniforms uniforms; uniforms.iTime=0.9;
            program->Draw(uniforms,context.functions()); const auto first=program->Image();
            uniforms.iTime=3.7; program->Draw(uniforms,context.functions()); const auto second=program->Image();
            Check(first.width()==800&&first.height()==500,"wrong output resolution");
            Check(first!=second,"animation did not advance");
            unsigned lit=0,changed=0;
            for(int y=0;y<500;y+=4) for(int x=0;x<800;x+=4) {
                const auto c=second.pixel(x,y); if(qRed(c)+qGreen(c)+qBlue(c)>12) ++lit;
                if(first.pixel(x,y)!=c) ++changed;
            }
            Check(lit>100&&changed>100,"insufficient visible/animated samples");
            Check(context.functions()->glGetError()==GL_NO_ERROR,"GL error");
            evidence.push_back({{"profile",data["profile_name"]},{"lit_samples",lit},{"changed_samples",changed}});
            painter.drawImage(QRect((index%2)*400,(index/2)*270,400,250),second);
            painter.setPen(Qt::white); painter.drawText((index%2)*400+8,(index/2)*270+265,file);
            ++index; program->CleanupGL(); delete program;
        }
        painter.end(); QDir().mkpath(output); Check(sheet.save(output+"/contact-sheet.png"),"image save failed");
        std::ofstream(QDir(output).filePath("gpu-validation.json").toStdString())<<evidence.dump(2)<<'\n';
        std::cout<<"PASS six GLSL110 presets: production renderer, 800x500, spatial color and animation\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
int main(int argc,char** argv)
{
    if(argc!=3) return 2; QGuiApplication app(argc,argv); int result=3; std::thread worker;
    QTimer::singleShot(0,&app,[&]{worker=std::thread([&]{result=Render(argv[1],argv[2]);QMetaObject::invokeMethod(&app,&QGuiApplication::quit,Qt::QueuedConnection);});});
    QTimer::singleShot(20000,&app,[]{std::_Exit(4);}); app.exec(); if(worker.joinable())worker.join(); return result;
}
