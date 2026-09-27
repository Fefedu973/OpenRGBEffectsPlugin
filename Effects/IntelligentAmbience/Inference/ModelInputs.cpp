// SPDX-License-Identifier: GPL-2.0-or-later
#include "ModelInputs.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace room_ai::inference
{
namespace
{
constexpr double MaximumGap=.250;
constexpr std::uint64_t MaximumModelPixels=256ULL*256;
const TensorSpec* Find(const std::vector<TensorSpec>& values,const char* name)
{
    const TensorSpec* result=nullptr;
    for(const auto& value:values) if(value.name==name)
    { if(result) return nullptr; result=&value; }
    return result;
}
const Tensor* Find(const std::vector<Tensor>& values,const char* name)
{
    const Tensor* result=nullptr;
    for(const auto& value:values) if(value.name==name)
    { if(result) return nullptr; result=&value; }
    return result;
}
bool ImageShape(const TensorSpec* spec,int channels,std::uint64_t maximum)
{
    return spec && spec->shape.size()==4 && spec->shape[0]==1 && spec->shape[1]==channels &&
        spec->shape[2]>0 && spec->shape[2]<=256 && spec->shape[3]>0 && spec->shape[3]<=256 &&
        std::uint64_t(spec->shape[2])*std::uint64_t(spec->shape[3])<=maximum;
}
bool RectValid(const std::array<double,4>& rect)
{
    // Off-canvas calibration is legal, but unbounded/degenerate geometry is not.
    return std::all_of(rect.begin(),rect.end(),[](double v){return std::isfinite(v)&&std::abs(v)<=16;}) &&
        rect[2]>0 && rect[3]>0;
}
bool AgeValid(const ModelConfig& config)
{return config.max_age_ms>=20 && config.max_age_ms<=2000;}
Tensor MakeTensor(const TensorSpec& spec,std::vector<float> values)
{Tensor out;out.name=spec.name;out.shape=spec.shape;out.values=std::move(values);return out;}
const std::array<float,256>& DecodeTable()
{
    static const auto table=[] {std::array<float,256> out{};for(unsigned i=0;i<256;++i)
    {const float v=float(i)/255.f;out[i]=v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);}return out;}();
    return table;
}
struct Weight {int pixel;double weight;};
using Axis=std::vector<std::vector<Weight>>;
Axis Weights(int source,int target)
{
    Axis result{std::size_t(target)};
    const double scale=double(source)/target;
    for(int i=0;i<target;++i)
    {
        auto& weights=result[std::size_t(i)];
        if(scale>=1)
        {
            const double begin=i*scale,end=(i+1)*scale;
            for(int pixel=int(std::floor(begin));pixel<std::min(source,int(std::ceil(end)));++pixel)
            {
                const double area=std::max(0.,std::min(end,double(pixel+1))-std::max(begin,double(pixel)));
                if(area>0) weights.push_back({pixel,area/scale});
            }
        }
        else
        {
            const double center=std::clamp((i+.5)*scale-.5,0.,double(source-1));
            const int first=int(std::floor(center)),second=std::min(source-1,first+1);
            weights.push_back({first,1-(center-first)});
            if(second!=first) weights.push_back({second,center-first});
        }
    }
    return result;
}
std::vector<float> LinearNchw(const QImage& source,int width,int height)
{
    // RGBA8888 is straight alpha, including when the source is premultiplied.
    // No Qt scaling in sRGB occurs: each sample is decoded before integration.
    const auto image=source.convertToFormat(QImage::Format_RGBA8888);
    if(image.isNull()) return {};
    const auto xs=Weights(image.width(),width),ys=Weights(image.height(),height);
    const auto& decode=DecodeTable();
    const std::size_t plane=std::size_t(width)*std::size_t(height);
    std::vector<float> output(plane*3);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)
    {
        std::array<double,3> color{};
        for(const auto& yw:ys[std::size_t(y)])for(const auto& xw:xs[std::size_t(x)])
        {
            const auto* pixel=image.constScanLine(yw.pixel)+xw.pixel*4;
            const double weight=xw.weight*yw.weight*double(pixel[3])/255.;
            for(unsigned c=0;c<3;++c)color[c]+=decode[pixel[c]]*weight;
        }
        const std::size_t index=std::size_t(y)*std::size_t(width)+std::size_t(x);
        for(unsigned c=0;c<3;++c)output[c*plane+index]=float(color[c]);
    }
    return output;
}
}

void VideoAdapter::Reset()
{previous.clear();shape.clear();rect={};generation=epoch=sequence=0;source_width=source_height=0;time=0;}
std::optional<Request> VideoAdapter::Make(const ModelConfig& config,const QImage& image,
    std::uint64_t next_generation,std::uint64_t source_epoch,std::uint64_t next_sequence,
    double source_time,const std::array<double,4>& screen)
{
    const auto* current=Find(config.inputs,"current_rgb");
    const auto* prior=Find(config.inputs,"previous_rgb");
    const auto* delta=Find(config.inputs,"delta_seconds");
    const auto* geometry=Find(config.inputs,"screen_rect");
    if(config.task!="video-field-v1" || config.inputs.size()!=4 || !ImageShape(current,3,MaximumModelPixels) ||
       !prior || prior->shape!=current->shape || !delta || delta->shape!=std::vector<std::int64_t>{1} ||
       !geometry || geometry->shape!=std::vector<std::int64_t>{1,4} || !AgeValid(config) ||
       !next_generation || !source_epoch || !next_sequence || !std::isfinite(source_time) || source_time<0 ||
       !RectValid(screen) || image.isNull() || image.width()>4096 || image.height()>4096 ||
       std::uint64_t(image.width())*std::uint64_t(image.height())>16ULL*1024*1024)
    {Reset();return {};}
    const bool changed=previous.empty() || next_generation!=generation || source_epoch!=epoch ||
        current->shape!=shape || screen!=rect || image.width()!=source_width || image.height()!=source_height;
    if(!changed && next_sequence<=sequence) return {};
    if(!changed && source_time<=time) {Reset();return {};}
    auto pixels=LinearNchw(image,int(current->shape[3]),int(current->shape[2]));
    if(pixels.empty()) {Reset();return {};}
    bool reset=changed || source_time-time>MaximumGap;
    if(!reset)
    {
        double difference=0;
        for(std::size_t i=0;i<pixels.size();++i)difference+=std::abs(pixels[i]-previous[i]);
        // Conservative full-frame discontinuity heuristic, never semantic detection.
        reset=difference/double(pixels.size())>.35;
    }
    Request request;request.generation=next_generation;request.epoch=source_epoch;
    request.sequence=next_sequence;request.source_time=source_time;request.reset=reset;
    std::vector<float> rectangle;for(double value:screen)rectangle.push_back(float(value));
    for(const auto& input:config.inputs)
    {
        if(input.name=="current_rgb")request.inputs.push_back(MakeTensor(input,pixels));
        else if(input.name=="previous_rgb")request.inputs.push_back(MakeTensor(input,reset?pixels:previous));
        else if(input.name=="delta_seconds")request.inputs.push_back(MakeTensor(input,{float(reset?0:source_time-time)}));
        else request.inputs.push_back(MakeTensor(input,rectangle));
    }
    previous=std::move(pixels);shape=current->shape;rect=screen;generation=next_generation;epoch=source_epoch;
    sequence=next_sequence;time=source_time;source_width=image.width();source_height=image.height();
    return request;
}

std::size_t AudioSamples(const ModelConfig& config)
{
    const auto* pcm=Find(config.inputs,"audio_pcm");
    if(config.task!="audio-field-v1" || config.inputs.size()!=1 || !pcm || pcm->shape.size()!=3 ||
       pcm->shape[0]!=1 || pcm->shape[1]!=1 || pcm->shape[2]<480 || pcm->shape[2]>240000 || !AgeValid(config))return 0;
    return std::size_t(pcm->shape[2]);
}
void AudioAdapter::Reset(){generation=epoch=sequence=0;samples=0;time=0;}
std::optional<Request> AudioAdapter::Make(const ModelConfig& config,const room_audio::PcmWindowSnapshot& pcm,
    std::uint64_t next_generation,double now)
{
    const auto count=AudioSamples(config);
    if(!count || !next_generation || !pcm.Ready() || !pcm.epoch || !pcm.sequence || pcm.sample_rate!=48000 ||
       pcm.mono.size()!=count || !std::isfinite(now) || !std::isfinite(pcm.source_begin) ||
       !std::isfinite(pcm.source_end) || !std::isfinite(pcm.captured_through) || pcm.source_begin<0 ||
       pcm.source_end>now || pcm.source_end>pcm.captured_through+1e-6 ||
       now-pcm.source_end>std::min(.150,config.max_age_ms/1000.) ||
       std::abs(pcm.source_end-pcm.source_begin-double(count)/48000.)>1e-6 ||
       !std::all_of(pcm.mono.begin(),pcm.mono.end(),[](float v){return std::isfinite(v)&&v>=-1&&v<=1;}))
    {Reset();return {};}
    const bool changed=!sequence || generation!=next_generation || epoch!=pcm.epoch || samples!=count;
    if(!changed && pcm.sequence<=sequence)return {};
    if(!changed && pcm.source_end<=time){Reset();return {};}
    Request request;request.generation=next_generation;request.epoch=pcm.epoch;request.sequence=pcm.sequence;
    request.source_time=pcm.source_end;request.reset=changed||pcm.source_end-time>MaximumGap;
    request.inputs.push_back(MakeTensor(config.inputs.front(),pcm.mono));
    generation=next_generation;epoch=pcm.epoch;sequence=pcm.sequence;samples=count;time=pcm.source_end;
    return request;
}

std::shared_ptr<const DynamicShaderImage> FieldImage(const Result& result,const ModelConfig& config,double now)
{
    const auto* shape=Find(config.outputs,"field_rgb");
    const auto* rgb=Find(result.outputs,"field_rgb");
    const auto* alpha_shape=Find(config.outputs,"confidence");
    const auto* alpha=Find(result.outputs,"confidence");
    if((config.task!="video-field-v1" && config.task!="audio-field-v1") || !AgeValid(config) || !RectValid(config.field_rect) ||
       !ImageShape(shape,3,MaximumModelPixels) || !rgb || rgb->shape!=shape->shape ||
       result.outputs.size()!=config.outputs.size() || config.outputs.size()!=(alpha_shape?2u:1u) ||
       !result.generation || !result.epoch || !result.sequence || !std::isfinite(now) ||
       !std::isfinite(result.source_time) || !std::isfinite(result.completed_time) || !std::isfinite(result.expires) ||
       result.source_time<0 || result.source_time>result.completed_time || result.completed_time>now ||
       result.expires<result.completed_time || result.expires<=now)return {};
    const std::size_t count=std::size_t(shape->shape[2])*std::size_t(shape->shape[3]);
    if(rgb->values.size()!=count*3 ||
       !std::all_of(rgb->values.begin(),rgb->values.end(),[](float v){return std::isfinite(v);}))return {};
    if(alpha_shape && (!ImageShape(alpha_shape,1,MaximumModelPixels) ||
       alpha_shape->shape[2]!=shape->shape[2] || alpha_shape->shape[3]!=shape->shape[3] ||
       !alpha || alpha->shape!=alpha_shape->shape || alpha->values.size()!=count ||
       !std::all_of(alpha->values.begin(),alpha->values.end(),[](float v){return std::isfinite(v);})))return {};
    if(!alpha_shape && alpha)return {};
    const double expiry=std::min(result.expires,result.source_time+config.max_age_ms/1000.);
    if(expiry<=now)return {};
    auto values=std::make_shared<std::vector<float>>(count*4);
    for(std::size_t i=0;i<count;++i)
    {
        for(unsigned c=0;c<3;++c)(*values)[i*4+c]=std::clamp(rgb->values[c*count+i],0.f,1.f);
        (*values)[i*4+3]=alpha?std::clamp(alpha->values[i],0.f,1.f):1;
    }
    auto image=std::make_shared<DynamicShaderImage>();image->width=unsigned(shape->shape[3]);image->height=unsigned(shape->shape[2]);
    image->rgba32f=std::move(values);image->generation=result.generation;image->sequence=result.sequence;image->source_revision=result.epoch;
    image->expires=std::chrono::steady_clock::time_point(std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(expiry)));
    return image;
}
}
