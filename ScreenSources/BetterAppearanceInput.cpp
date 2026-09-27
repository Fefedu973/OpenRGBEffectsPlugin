// SPDX-License-Identifier: GPL-2.0-or-later
#include "BetterAppearanceInput.h"
#include "Effects/BetterCapture/Appearance.h"
#include <QFile>
#include <QJsonDocument>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace better_source
{
namespace
{
std::string Resource(const std::string& name)
{
    // Paths come only from the built-in graph generator, never producer JSON.
    if(name.find("..")!=std::string::npos || name.rfind("shaders/BetterCapture/",0)!=0)
        throw std::invalid_argument("Invalid native appearance resource");
    QFile file(":"+QString::fromStdString("/"+name));
    if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Native appearance shader resource missing");
    return file.readAll().toStdString();
}
}
class AppearanceInput::Impl
{
public:
    std::shared_ptr<FrameSource> source;
    std::atomic<bool> stop{false};
    mutable std::mutex mutex;
    std::condition_variable wake;
    unsigned width,height;
    std::shared_ptr<const ShaderRenderGraphFrame> frame=std::make_shared<ShaderRenderGraphFrame>();
    QString detail;
    std::thread worker;
    Impl(std::shared_ptr<FrameSource> input,unsigned w,unsigned h):source(std::move(input)),width(w),height(h),worker([this]{Run();}){}
    ~Impl(){stop=true;wake.notify_all();worker.join();}
    void Run()
    {
        QJsonObject previous,failed_recipe;unsigned last_width=0,last_height=0,failed_width=0,failed_height=0;
        bool failed=false;
        std::shared_ptr<const ShaderRenderGraph> graph;
        std::shared_ptr<const DynamicShaderImage> geometry;
        while(!stop)
        {
            unsigned w,h;{std::lock_guard<std::mutex> lock(mutex);w=width;h=height;}
            auto next=std::make_shared<ShaderRenderGraphFrame>();QString message;
            try
            {
                const auto input=source->Read();
                message=input->detail;
                if(input->Usable())
                {
                    auto recipe=input->metadata;recipe.remove("stateRevision");
                    if(!graph || recipe!=previous || w!=last_width || h!=last_height)
                    {
                        if(failed && recipe==failed_recipe && w==failed_width && h==failed_height)
                            throw std::invalid_argument("Unchanged unsupported appearance recipe");
                        failed=true;failed_recipe=recipe;failed_width=w;failed_height=h;
                        const auto json=nlohmann::json::parse(QJsonDocument(input->metadata).toJson(QJsonDocument::Compact).toStdString());
                        const auto prepared=better_capture::Prepare(json,w,h);
                        auto compiled=std::make_shared<ShaderRenderGraph>();compiled->output=prepared.graph.output;
                        const auto common=Resource(prepared.graph.preamble);
                        std::uint64_t intermediate_bytes=0;
                        for(const auto& p:prepared.graph.passes)
                        {
                            intermediate_bytes+=std::uint64_t(p.width)*p.height*8;
                            if(intermediate_bytes>128ULL*1024*1024)throw std::invalid_argument("Native appearance graph exceeds memory budget");
                            ShaderRenderGraph::Pass pass;pass.id=p.id;pass.inputs=p.inputs;pass.width=p.width;pass.height=p.height;
                            pass.fragment=common+"\n"+Resource(p.shader);
                            for(const auto& u:p.uniforms)pass.uniforms[u.first]={u.second.value,int(u.second.components)};
                            compiled->passes.push_back(std::move(pass));
                        }
                        auto atlas=std::make_shared<DynamicShaderImage>();atlas->rgba32f=prepared.geometryRGBA;
                        atlas->width=prepared.geometryWidth;atlas->height=prepared.geometryHeight;atlas->sequence=1;
                        graph=std::move(compiled);geometry=std::move(atlas);previous=recipe;last_width=w;last_height=h;
                        failed=false;
                    }
                    auto raw=std::make_shared<DynamicShaderImage>();raw->image=input->raw;
                    raw->generation=input->raw_generation;raw->sequence=input->raw_sequence;raw->expires=input->expires;
                    auto coverage=std::make_shared<DynamicShaderImage>();coverage->image=input->coverage;
                    coverage->generation=input->coverage_generation;coverage->sequence=input->coverage_sequence;coverage->expires=input->expires;
                    next->graph=graph;next->images={{"raw",raw},{"coverage",coverage}};
                    if(geometry && geometry->Valid())next->images["geometry"]=geometry;
                    next->expires=input->expires;
                    message=QStringLiteral("Native appearance follows Better settings (%1 × %2)").arg(w).arg(h);
                }
            }
            catch(const std::exception&){message=QStringLiteral("Native appearance recipe is invalid or unsupported; output is black.");}
            std::unique_lock<std::mutex> lock(mutex);frame=std::move(next);detail=message;
            wake.wait_for(lock,std::chrono::milliseconds(16),[this]{return stop.load();});
        }
    }
};
AppearanceInput::AppearanceInput(std::shared_ptr<FrameSource> source,unsigned width,unsigned height)
    :impl(std::make_unique<Impl>(std::move(source),width,height)){}
AppearanceInput::~AppearanceInput()=default;
std::shared_ptr<const ShaderRenderGraphFrame> AppearanceInput::Read()const{std::lock_guard<std::mutex> lock(impl->mutex);return impl->frame;}
QString AppearanceInput::Status()const{std::lock_guard<std::mutex> lock(impl->mutex);return impl->detail;}
void AppearanceInput::Resize(unsigned width,unsigned height)
{std::lock_guard<std::mutex> lock(impl->mutex);impl->width=width;impl->height=height;impl->wake.notify_all();}
}
