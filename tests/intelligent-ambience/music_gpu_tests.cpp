// SPDX-License-Identifier: GPL-2.0-or-later
// Real production shader pipeline with synthetic audio observations only.
#include "Effects/IntelligentAmbience/MusicDirector.h"
#include "Effects/Shaders/ShaderProgram.h"
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QFile>
#include <iostream>
#include <stdexcept>

namespace
{
unsigned checks=0,frames=0;int maximum_error=0;
void Check(bool ok,const char*text){++checks;if(!ok)throw std::runtime_error(text);}
Uniforms Bind(const room_ai::MusicDirector&director,double now,bool directed,float height)
{
    Uniforms u;const auto state=director.RenderState(now);
    u.custom["iaMusicPalette"]={{{state.palette.r,state.palette.g,state.palette.b,0}},3};
    u.custom["iaMusicControls"]={state.controls,4};
    u.custom["iaMusicMotionPhase"]={{{state.motion_phase,0,0,0}},1};
    u.custom["iaMusicDirected"]={{{directed?1.f:0.f,0,0,0}},1};
    u.custom["iaScreenHeight"]={{{height,0,0,0}},1};
    u.custom["iaMusicBands0"]={{{state.bands[0],state.bands[1],state.bands[2],state.bands[3]}},4};
    u.custom["iaMusicBands1"]={{{state.bands[4],state.bands[5],state.bands[6],state.bands[7]}},4};
    for(unsigned i=0;i<16;++i)u.custom["iaMusicAccents["+std::to_string(i)+"]"]={state.accents[i],4};
    return u;
}
QImage Compare(ShaderProgram&program,QOpenGLFunctions*gl,const room_ai::MusicDirector&director,
               double now,bool directed,float height,const char*label)
{
    program.Draw(Bind(director,now,directed,height),gl);const auto image=program.Image();int error=0;
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)
    {
        room_ai::Vec2 point{(x+.5)/image.width()*2-.5,(y+.5)/image.height()*.9-.1};
        // CPU reference has height0.5625; map only its y coordinate when testing
        // the shader's independently configurable reference aspect ratio.
        if(!directed)point.y*=.5625/height;
        const auto expected=director.Sample(point,now,directed);const auto actual=image.pixelColor(x,y);
        const int values[]={int(std::lround(expected.r*255)),int(std::lround(expected.g*255)),int(std::lround(expected.b*255))};
        const int observed[]={actual.red(),actual.green(),actual.blue()};
        for(unsigned c=0;c<3;++c)error=std::max(error,std::abs(values[c]-observed[c]));
    }
    ++frames;maximum_error=std::max(maximum_error,error);std::cout<<label<<" max linear8 error="<<error<<'\n';
    Check(error<=1,"music shader differs from CPU by more than one byte");return image;
}
}
int main(int argc,char**argv)
{
    QGuiApplication app(argc,argv);
    try
    {
        Check(argc==2,"supply music.fs path");QFile file(QString::fromLocal8Bit(argv[1]));
        Check(file.open(QIODevice::ReadOnly),"music fragment readable");
        QOffscreenSurface surface;surface.create();QOpenGLContext context;context.setFormat(surface.format());
        Check(context.create()&&context.makeCurrent(&surface),"offscreen GL available");auto*gl=context.functions();
        ShaderProgram program;program.SetVersion("130");program.Resize(320,200);
        program.main_pass->data.fragment_shader="uniform float iaScreenHeight;\n"+file.readAll().toStdString()+R"GLSL(
void mainImage(out vec4 color,in vec2 pixel)
{
    vec2 world=vec2(pixel.x/iResolution.x*2.0-0.5,(1.0-pixel.y/iResolution.y)*0.9-0.1);
    color=vec4(iaMusicSample(world),1);
}
)GLSL";
        program.Init();const auto errors=program.Compile();if(!errors.isEmpty())std::cerr<<errors.toStdString()<<'\n';
        Check(errors.isEmpty(),"GLSL130 production compile");
        room_ai::MusicDirector director;
        Compare(program,gl,director,1000,true,.5625f,"empty directed");
        Compare(program,gl,director,1000,false,.5625f,"empty reference");
        room_audio::RhythmSnapshot r;r.generation=1;r.sequence=1;r.audio_time=1000;r.silent=false;
        r.power=.25f;r.spectrum[1]=.09f;r.spectrum[4]=.36f;r.spectrum[12]=.81f;r.spectrum[155]=1;
        r.locked=true;r.confidence=.85f;r.bpm=120;r.phase=.95f;director.Push(r,1000);
        const auto directed=Compare(program,gl,director,1000,true,.5625f,"directed");
        const auto reference=Compare(program,gl,director,1000,false,.5625f,"eight bands reference");
        Check(directed!=reference,"directed control visibly selects a different rendering");
        Compare(program,gl,director,1000,false,.9f,"reference tall aspect");
        Compare(program,gl,director,1000,false,.25f,"reference short aspect");
        r.sequence=2;r.audio_time=1000.01;r.onset_sequence=1;r.last_onset_time=r.audio_time;r.onset_strength=1;
        director.Push(r,r.audio_time);
        Compare(program,gl,director,1000.01,true,.5625f,"transient accent");
        Compare(program,gl,director,1000.11,true,.5625f,"decayed accent and wrapped phase");
        Compare(program,gl,director,1000.2,true,.5625f,"stale directed");
        Compare(program,gl,director,1000.2,false,.5625f,"stale reference");
        director.SetIntensity(0);Compare(program,gl,director,1000.01,false,.5625f,"zero intensity reference");
        director.SetIntensity(.6);r.silent=true;director.Push(r,1000.02);
        Compare(program,gl,director,1000.02,false,.5625f,"same-hop silence reference");
        Compare(program,gl,director,1000.02,true,.5625f,"same-hop silence directed");
        Check(gl->glGetError()==GL_NO_ERROR,"no GL errors");program.CleanupGL();
        std::cout<<"Music GPU: "<<checks<<" checks, "<<frames<<" full-frame comparisons, max_linear8_error="<<maximum_error<<'\n';return 0;
    }
    catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
