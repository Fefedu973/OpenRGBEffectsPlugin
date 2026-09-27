// SPDX-License-Identifier: GPL-2.0-or-later
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QTimer>
#include "ShaderProgram.h"
#include "MusicEnvelope.h"
#include "RhythmEnvelope.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
static unsigned checks=0;
static void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
static int Light(const QImage& image,double x,double y)
{auto p=image.pixel(int(x*2.5),int(y*2.5));return std::max({qRed(p),qGreen(p),qBlue(p)});}
static int Render(const char* profile,const char* output)
{
    try
    {
        QOffscreenSurface surface;surface.create();QOpenGLContext context;context.setFormat(surface.format());
        Check(context.create()&&context.makeCurrent(&surface),"offscreen GL context");
        std::ifstream file(profile);json data;file>>data;
        auto* program=ShaderProgram::FromJSON(data["plugins"]["OpenRGB Effects Plugin"]["Effects"][0]["CustomSettings"]["shader_program"]);
        program->Init();Check(program->Compile().isEmpty(),"Room Pulse GLSL110 compiles through production ShaderPass");
        std::array<float,256> audio{};Uniforms uniforms;uniforms.iAudio=audio.data();uniforms.iTime=2;
        program->Draw(uniforms,context.functions());auto silent=program->Image();
        Check(silent.width()==800&&silent.height()==500,"800x500 output");
        Check(Light(silent,20,100)<4&&Light(silent,200,15)<4&&Light(silent,184,107)<4,"silence keeps reactive regions dark");
        Check(Light(silent,305,5)<8&&Light(silent,305,15)<4,"ring stays dim in silence rather than permanently bright");
        silent.save(QString::fromStdString(std::string(output)+"/room-pulse-silent.png"));

        // Known uniforms verify placement independently of the envelope test.
        uniforms.iMusic={0.5f,0.25f,0.12f,0.0f};
        uniforms.iRhythm={120.0f,0.5f,0.9f,0.25f};
        uniforms.iOnset={0.1f,0.42f,0.3f,0.5f};
        for(int bin=0;bin<64;++bin)
        {float v=0.06f+float((bin*13)%23)/35.0f;std::fill(audio.begin()+bin*4,audio.begin()+bin*4+4,v);}
        program->Draw(uniforms,context.functions());auto image=program->Image();
        Check(Light(image,24,160)>200&&Light(image,24,50)<8,"left broadband VU fills upward");
        Check(Light(image,60,15)>200&&Light(image,120,15)<8,"top bass VU remains separate from tracked pulse");
        Check(Light(image,200,15)>50&&Light(image,200,15)<80,"pulse tile follows audio-clock pulse");
        Check(Light(image,240,15)>95&&Light(image,240,15)<120,"midrange attack has an independent region");
        Check(Light(image,266,15)>200&&Light(image,284,15)<8,"fan sector follows locked half-cycle phase");
        Check(Light(image,305,15)>65&&Light(image,305,15)<90,"ring includes high-frequency accents");
        unsigned mirrored=0;double mirror_error=0;
        for(int y=80;y<460;y+=3)for(int x=125;x<460;x+=3)
        {
            // GL pixel centers: X=184*2.5, Y=107.5*2.5. Vertical axis falls
            // between half-pixels here, so compare only exact horizontal pairs.
            auto a=image.pixel(x,y),b=image.pixel(919-x,y);
            mirror_error+=abs(qRed(a)-qRed(b))+abs(qGreen(a)-qGreen(b))+abs(qBlue(a)-qBlue(b));++mirrored;
        }
        Check(mirror_error/(3*mirrored)<1.0,"central spectrum mirrors around design X184");
        image.save(QString::fromStdString(std::string(output)+"/room-pulse-spectrum.png"));

        auto before=image;
        uniforms.iTime=20;program->Draw(uniforms,context.functions());auto timeOnly=program->Image();
        Check(before==timeOnly,"RandomBeat color does not advance on time alone");
        uniforms.iMusic[2]=0.65f;program->Draw(uniforms,context.functions());auto newHue=program->Image();
        Check(before.pixel(60,400)!=newHue.pixel(60,400),"onset hue reaches volume strip");
        Check(before.pixel(500,37)!=newHue.pixel(500,37),"onset hue reaches bass tile");

        // Integrate the actual state with the actual shader for a synthetic
        // attack; no capture endpoint or hardware device is opened.
        MusicEnvelope envelope; RhythmEnvelope rhythmic; audio.fill(0);envelope.Update(audio.data(),0);
        room_audio::RhythmSnapshot beat;beat.sequence=1;beat.generation=1;beat.silent=false;beat.audio_time=10;
        rhythmic.Update(beat,10);
        for(int bin=1;bin<=3;++bin)std::fill(audio.begin()+bin*4,audio.begin()+bin*4+4,0.65f);
        uniforms.iMusic=envelope.Update(audio.data(),0.2);
        beat.sequence=2;beat.audio_time=10.2;beat.onset_sequence=1;beat.last_onset_time=10.19;
        const auto pulse=rhythmic.Update(beat,10.2);uniforms.iRhythm=pulse.rhythm;uniforms.iOnset=pulse.transients;
        program->Draw(uniforms,context.functions());
        auto bass=program->Image();Check(Light(bass,200,15)>200,"real rhythm envelope and shader produce a fresh transient pulse");
        bass.save(QString::fromStdString(std::string(output)+"/room-pulse-bass.png"));
        uniforms.iAudio=nullptr;uniforms.iMusic=envelope.Update(nullptr,0.3);
        beat.silent=true;const auto absent=rhythmic.Update(beat,10.3);uniforms.iRhythm=absent.rhythm;uniforms.iOnset=absent.transients;
        program->Draw(uniforms,context.functions());auto disconnected=program->Image();
        Check(Light(disconnected,184,107)<4&&Light(disconnected,200,15)<4,"missing audio clears prior GPU spectrum and envelope");

        // A quiet, isolated real DSP bin used to disappear after multiplication
        // by volume derived from the same spectrum. Exercise both shader
        // variants with the production MusicEnvelope and unchanged gain.
        std::array<float,256> quietAudio{};
        constexpr unsigned quietBin=20;
        std::fill_n(quietAudio.begin()+quietBin*4,4,0.05f);
        MusicEnvelope quietEnvelope;Uniforms quietUniforms;
        quietUniforms.iAudio=quietAudio.data();
        quietUniforms.iMusic=quietEnvelope.Update(quietAudio.data(),30.0);
        const double quietLevel=std::pow(0.05*1.8,0.65);
        const double oldLevel=quietLevel*quietUniforms.iMusic[0]*0.85;
        Check(std::abs(quietLevel-0.209054)<0.00001&&std::abs(oldLevel-0.002888)<0.00001,
              "quiet bin reproduces 0.0029 old versus 0.209 unattenuated spectrum level");
        program->Draw(quietUniforms,context.functions());const auto quiet=program->Image();
        const double quietX=184.0+(quietBin+0.5)*136.0/64.0;
        Check(Light(quiet,quietX,120)>245,"quiet spectrum bar remains visible above its center");
        const int quietLine=Light(quiet,quietX,192);
        Check(std::abs(quietLine-quietLevel*1.5*255)<2,"quiet lower strip preserves measured bin magnitude");
        Check(Light(quiet,24,190)<4&&Light(quiet,24,199)>245,
              "quiet broadband VU retains its true short fill, not an artificial gain");
        Check(Light(quiet,200,15)<4,"quiet static tone does not invent a rhythmic pulse");

        auto oldSettings=data["plugins"]["OpenRGB Effects Plugin"]["Effects"][0]["CustomSettings"]["shader_program"];
        std::string oldSource=oldSettings["main_pass"]["fragment_shader"];
        const std::string corrected="const bool SCALE_SPECTRUM_BY_VOLUME = false;";
        const auto position=oldSource.find(corrected);
        Check(position!=std::string::npos,"test profile uses the corrected production shader");
        oldSource.replace(position,corrected.size(),"const bool SCALE_SPECTRUM_BY_VOLUME = true;");
        oldSettings["main_pass"]["fragment_shader"]=oldSource;
        auto* oldProgram=ShaderProgram::FromJSON(oldSettings);
        oldProgram->Init();Check(oldProgram->Compile().isEmpty(),"old attenuation control variant compiles");
        oldProgram->Draw(quietUniforms,context.functions());const auto oldQuiet=oldProgram->Image();
        const int oldQuietLine=Light(oldQuiet,quietX,192);
        Check(Light(oldQuiet,quietX,120)<4&&oldQuietLine<4,
              "negative control reproduces the former bar and strip disappearing into background");
        quiet.save(QString::fromStdString(std::string(output)+"/room-pulse-quiet.png"));
        oldQuiet.save(QString::fromStdString(std::string(output)+"/room-pulse-quiet-old.png"));
        oldProgram->CleanupGL();delete oldProgram;
        Check(context.functions()->glGetError()==GL_NO_ERROR,"no GL errors");
        json evidence={{"GL",reinterpret_cast<const char*>(context.functions()->glGetString(GL_VERSION))},
                       {"assertions",checks},{"width",800},{"height",500},{"audio_capture_opened",false},
                       {"horizontal_mirror_mean_error",mirror_error/(3*mirrored)},
                       {"quiet_bin",quietBin},{"quiet_magnitude",0.05},
                       {"old_spectrum_level",oldLevel},{"corrected_spectrum_level",quietLevel},
                       {"old_strip_max_channel",oldQuietLine},{"corrected_strip_max_channel",quietLine}};
        std::ofstream(std::string(output)+"/room-pulse-validation.json")<<evidence.dump(2)<<'\n';
        program->CleanupGL();delete program;
        std::cout<<"PASS "<<checks<<" Room Pulse GPU checks\n";return 0;
    }
    catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
int main(int argc,char** argv)
{
    if(argc!=3)return 2;QGuiApplication app(argc,argv);int result=3;std::thread worker;
    QTimer::singleShot(0,&app,[&]{worker=std::thread([&]{result=Render(argv[1],argv[2]);QMetaObject::invokeMethod(&app,&QGuiApplication::quit,Qt::QueuedConnection);});});
    QTimer::singleShot(15000,&app,[]{std::_Exit(4);});app.exec();if(worker.joinable())worker.join();return result;
}
