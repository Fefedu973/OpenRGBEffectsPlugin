// SPDX-License-Identifier: GPL-2.0-or-later
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QTimer>
#include "ShaderProgram.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>

static unsigned checks=0;
static void Check(bool v,const char* s){++checks;if(!v)throw std::runtime_error(s);}
static double Delta(const QImage& a,const QImage& b,int column)
{
    double sum=0;unsigned count=0;
    for(int y=0;y<a.height();++y)for(int x=column*a.width()/3+2;x<(column+1)*a.width()/3-2;++x)
    {auto p=a.pixel(x,y),q=b.pixel(x,y);sum+=abs(qRed(p)-qRed(q))+abs(qGreen(p)-qGreen(q))+abs(qBlue(p)-qBlue(q));++count;}
    return sum/(3*count);
}
static double RegionDelta(const QImage& a,const QImage& b,int top,int bottom)
{
    double sum=0;unsigned count=0;
    for(int y=top;y<bottom;++y)for(int x=0;x<a.width();++x)
    {auto p=a.pixel(x,y),q=b.pixel(x,y);sum+=abs(qRed(p)-qRed(q))+abs(qGreen(p)-qGreen(q))+abs(qBlue(p)-qBlue(q));++count;}
    return sum/(3*count);
}
static int Light(const QImage& image,int x,int y)
{auto p=image.pixel(x,y);return std::max({qRed(p),qGreen(p),qBlue(p)});}
static int Render(const char* profile,const char* output)
{
    try
    {
        QOffscreenSurface surface;surface.create();QOpenGLContext context;context.setFormat(surface.format());
        Check(context.create()&&context.makeCurrent(&surface),"offscreen GL context");
        std::ifstream f(profile);json data;f>>data;
        auto* program=ShaderProgram::FromJSON(data["plugins"]["OpenRGB Effects Plugin"]["Effects"][0]["CustomSettings"]["shader_program"]);
        program->Init();auto log=program->Compile();
        Check(log.isEmpty(),"production ShaderPass GLSL110 compiles");
        std::array<float,256> spectrum{};Uniforms u;u.iTime=2.f;u.iAudio=spectrum.data();
        program->Draw(u,context.functions());QImage silent=program->Image();
        Check(silent.width()==800&&silent.height()==500,"native 800x500 image");
        json result={{"GL",reinterpret_cast<const char*>(context.functions()->glGetString(GL_VERSION))},{"width",800},{"height",500},{"bands",json::array()}};
        const unsigned begin[]={0,16,96},end[]={16,96,256};
        for(int band=0;band<3;++band)
        {
            spectrum.fill(0.f);std::fill(spectrum.begin()+begin[band],spectrum.begin()+end[band],0.75f);
            program->Draw(u,context.functions());QImage active=program->Image();
            json deltas=json::array();
            for(int column=0;column<3;++column)
            {
                double d=Delta(active,silent,column);deltas.push_back(d);
                Check(column==band ? d>20.0 : d==0.0,"synthetic FFT activates exactly its spatial column");
            }
            result["bands"].push_back(deltas);
            active.save(QString::fromStdString(std::string(output)+"/band-"+std::to_string(band)+".png"));
        }
        spectrum.fill(0.001f);program->Draw(u,context.functions());
        auto gated=program->Image();for(int col=0;col<3;++col)Check(Delta(gated,silent,col)==0.,"noise floor remains at idle");
        // Moving one true FFT magnitude within its band keeps band energy
        // identical. Only the upper spectrum must move; lower meters must not.
        const int firstBins[]={0,4,24},binCounts[]={4,20,40};
        const std::string fragment=data["plugins"]["OpenRGB Effects Plugin"]["Effects"][0]["CustomSettings"]["shader_program"]["main_pass"]["fragment_shader"];
        const bool paired=fragment.find("const int SPECTRUM_BARS = 32;")!=std::string::npos;
        result["spectrum_bars"]=paired ? 32 : 64;
        for(int band=0;band<3;++band)
        {
            spectrum.fill(0.f);
            std::fill(spectrum.begin()+firstBins[band]*4,spectrum.begin()+firstBins[band]*4+4,0.75f);
            program->Draw(u,context.functions());QImage first=program->Image();
            spectrum.fill(0.f);
            int lastBin=firstBins[band]+binCounts[band]-1;
            std::fill(spectrum.begin()+lastBin*4,spectrum.begin()+lastBin*4+4,0.75f);
            program->Draw(u,context.functions());QImage last=program->Image();
            Check(RegionDelta(first,last,0,250)>0.10,"individual FFT magnitude changes detailed upper spectrum");
            Check(RegionDelta(first,last,250,500)==0.,"equal band energy retains exact lower VU rendering");
            double bars=binCounts[band]/(paired?2.0:1.0);
            int firstX=int((band+0.5/bars)*800.0/3.0);
            int lastX=int((band+(bars-0.5)/bars)*800.0/3.0);
            Check(Light(first,firstX,145)>Light(last,firstX,145)+20,"first FFT bar is spatially resolved");
            Check(Light(last,lastX,145)>Light(first,lastX,145)+20,"last FFT bar is spatially resolved");
            first.save(QString::fromStdString(std::string(output)+"/spectrum-first-"+std::to_string(band)+".png"));
            last.save(QString::fromStdString(std::string(output)+"/spectrum-last-"+std::to_string(band)+".png"));
        }
        // Every supplied magnitude is a real DSP bin (four repeated slots).
        // Alternating values give a detailed full-spectrum evidence image.
        for(int bin=0;bin<64;++bin)
        {
            float magnitude=0.045f+float((bin*17)%31)/62.f;
            std::fill(spectrum.begin()+bin*4,spectrum.begin()+bin*4+4,magnitude);
        }
        program->Draw(u,context.functions());
        program->Image().save(QString::fromStdString(std::string(output)+"/detailed-spectrum.png"));
        spectrum.fill(0.5f);program->Draw(u,context.functions());auto at2=program->Image();
        u.iTime=4.f;program->Draw(u,context.functions());auto at4=program->Image();
        Check(Delta(at2,at4,0)>0.1,"audio-lit surface has spatial animated texture");
        at4.save(QString::fromStdString(std::string(output)+"/all-bands.png"));
        Check(context.functions()->glGetError()==GL_NO_ERROR,"no GL error");
        program->CleanupGL();delete program;
        result["assertions"]=checks;result["audio_capture_opened"]=false;
        std::ofstream(std::string(output)+"/shader-validation.json")<<result.dump(2)<<'\n';
        std::cout<<"PASS "<<checks<<" production shader assertions, synthetic spectrum only\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
int main(int argc,char** argv)
{
    if(argc!=3)return 2;QGuiApplication app(argc,argv);int result=3;std::thread worker;
    QTimer::singleShot(0,&app,[&]{worker=std::thread([&]{result=Render(argv[1],argv[2]);QMetaObject::invokeMethod(&app,&QGuiApplication::quit,Qt::QueuedConnection);});});
    QTimer::singleShot(15000,&app,[]{std::_Exit(4);});app.exec();if(worker.joinable())worker.join();return result;
}
