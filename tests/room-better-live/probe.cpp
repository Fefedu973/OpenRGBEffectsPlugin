// SPDX-License-Identifier: GPL-2.0-or-later
// Explicit live, read-only appearance diagnostic. No controllers or image files.
#include "ScreenSources/BetterDiscovery.h"
#include "ScreenSources/BetterFrameSource.h"
#include "ScreenSources/BetterAppearanceInput.h"
#include "Effects/Shaders/ShaderRenderGraph.h"
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>
#include <algorithm>
#include <iostream>
#include <vector>

int main(int argc,char** argv)
{
    QGuiApplication app(argc,argv);
    const auto args=app.arguments();QString descriptor;
    if(args.size()==3 && args[1]=="--descriptor" && QFileInfo(args[2]).isAbsolute())descriptor=args[2];
    else if(args.size()!=1){std::cerr<<"Usage: probe [--descriptor ABSOLUTE_PATH]\n";return 64;}
    QJsonObject report{{"read_only",true},{"image_export",false},{"hardware_output",false},{"ready",false}};
    QElapsedTimer elapsed;elapsed.start();bool ready=false;
    try
    {
        better_source::Config config;config.descriptor_path=descriptor;config.refresh_ms=500;config.request_timeout_ms=1000;
        auto discovery=better_source::Discovery::Acquire(config);
        auto source=better_source::FrameSource::Acquire(discovery);
        better_source::AppearanceInput appearance(source,800,600);
        QOffscreenSurface surface;surface.create();QOpenGLContext context;context.setFormat(surface.format());
        if(!surface.isValid() || !context.create() || !context.makeCurrent(&surface))throw std::runtime_error("GL unavailable");
        ShaderRenderGraphRunner renderer;
        std::vector<double> durations;
        unsigned draws=0,changed=0,nonblack=0,passes=0;QImage previous;
        std::shared_ptr<const better_source::FrameSnapshot> last;
        QString stage="waiting_pair";
        while(elapsed.elapsed()<14000 && draws<5)
        {
            QCoreApplication::processEvents();
            const auto pair=source->Read();const auto frame=appearance.Read();
            if(!pair->Usable()){stage="waiting_pair";QThread::msleep(10);continue;}
            if(!frame->graph || !frame->images.count("raw") || frame->images.at("raw")->generation!=pair->raw_generation ||
               std::chrono::steady_clock::now()>frame->expires)
            {stage="waiting_appearance";QThread::msleep(10);continue;}
            stage="rendering";QElapsedTimer draw;draw.start();
            auto image=renderer.Draw(*frame,800,600);
            durations.push_back(draw.nsecsElapsed()/1000000.0);
            const auto error=context.functions()->glGetError();
            if(error!=GL_NO_ERROR){report["gl_error"]=int(error);throw std::runtime_error("GL error");}
            if(image.isNull() || image.size()!=QSize(800,600))throw std::runtime_error("Output dimensions invalid");
            nonblack=0;
            for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)
                if(image.pixel(x,y)&0x00ffffffu)++nonblack;
            if(!previous.isNull() && image!=previous)++changed;
            previous=std::move(image);last=pair;passes=unsigned(frame->graph->passes.size());++draws;
            QThread::msleep(50);
        }
        const auto connection=discovery->Read();
        QJsonArray scenes;for(const auto& scene:connection->scenes)scenes.append(QJsonObject{{"id",scene.id},{"name",scene.name}});
        report["scenes"]=scenes;report["discovery_status"]=better_source::StateName(connection->state);
        report["frame_status"]=better_source::FrameStateName(source->Read()->state);
        report["appearance_status"]=appearance.Status();report["stage"]=stage;
        report["rendered_frames"]=int(draws);report["changed_frames"]=int(changed);report["nonblack_pixels_last"]=int(nonblack);
        report["graph_passes"]=int(passes);report["width"]=previous.width();report["height"]=previous.height();
        if(last)
        {
            report["raw_generation"]=QString::number(qulonglong(last->raw_generation));
            report["coverage_generation"]=QString::number(qulonglong(last->coverage_generation));
            report["schema"]=last->metadata.value("schema");report["schema_version"]=last->metadata.value("version");
        }
        if(!durations.empty())
        {
            const double first=durations.front();std::sort(durations.begin(),durations.end());
            report["render_ms"]=QJsonObject{{"first",first},{"median",durations[durations.size()/2]},{"maximum",durations.back()}};
        }
        ready=draws==5 && source->Read()->Usable();report["ready"]=ready;
        // Renderer is destroyed while its context is still current. The source
        // workers own no GL context and never mutate the producer's settings.
    }
    catch(const std::exception&){report["error"]="live_appearance_validation_failed";ready=false;report["ready"]=false;}
    report["elapsed_ms"]=double(elapsed.elapsed());
    std::cout<<QJsonDocument(report).toJson(QJsonDocument::Compact).constData()<<'\n';
    return ready?0:2;
}
