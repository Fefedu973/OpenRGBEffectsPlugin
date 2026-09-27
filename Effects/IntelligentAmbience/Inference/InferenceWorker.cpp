// SPDX-License-Identifier: GPL-2.0-or-later
#include "InferenceWorker.h"
#include "ModelGuard.h"
#include "vendor/onnxruntime_c_api.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace room_ai::inference
{
namespace
{
constexpr std::size_t MaxElements=1024*1024,MaxModelBytes=64*1024*1024;
void Require(bool good,const char* error){if(!good)throw std::runtime_error(error);}
std::size_t Count(const std::vector<std::int64_t>& shape)
{
    Require(!shape.empty()&&shape.size()<=4,"Tensor rank must be 1 through 4");
    std::size_t total=1;
    for(auto x:shape){Require(x>0&&std::uint64_t(x)<=MaxElements&&total<=MaxElements/std::size_t(x),"Tensor exceeds the static size limit");total*=std::size_t(x);}
    return total;
}
bool Finite(const std::vector<float>& v){return std::all_of(v.begin(),v.end(),[](float x){return std::isfinite(x);});}
QByteArray Read(const QString& path,qint64 limit)
{
    QFile file(path);Require(file.open(QIODevice::ReadOnly),"Cannot open package file");
    Require(file.size()>0&&file.size()<=limit,"Package file size outside limit");
    auto bytes=file.read(limit+1);Require(bytes.size()>0&&bytes.size()<=limit&&file.atEnd(),"Package changed while being read");return bytes;
}
bool Name(const std::string& s){return QRegularExpression("^[A-Za-z_][A-Za-z0-9_]{0,63}$").match(QString::fromStdString(s)).hasMatch();}
TensorSpec Spec(const Json& j)
{
    Require(j.is_object()&&j.size()==2&&j.contains("name")&&j.contains("shape"),"Tensor specification needs name and shape only");
    Require(j.at("shape").is_array()&&std::all_of(j.at("shape").begin(),j.at("shape").end(),[](const auto& d){return d.is_number_integer();}),"Dimensions must be integers");
    TensorSpec s{j.at("name").get<std::string>(),j.at("shape").get<std::vector<std::int64_t>>()};
    Require(Name(s.name),"Invalid tensor name");Count(s.shape);return s;
}
const TensorSpec& Named(const std::vector<TensorSpec>& list,const std::string& name)
{
    auto it=std::find_if(list.begin(),list.end(),[&](const auto& s){return s.name==name;});
    Require(it!=list.end(),"Required version-1 tensor is missing");return *it;
}
void ValidateTensor(const Tensor& t,const TensorSpec& s)
{Require(t.name==s.name&&t.shape==s.shape&&t.values.size()==Count(s.shape)&&Finite(t.values),"Tensor name, shape, size or finite-value check failed");}
std::shared_ptr<const ModelConfig> Parse(const Json& j)
{
    Require(j.is_object()&&j.at("schema_version")==1,"Unsupported model package schema");
    auto c=std::make_shared<ModelConfig>();c->manifest=j;c->id=j.at("id").get<std::string>();c->task=j.at("task").get<std::string>();
    Require(Name(c->id),"Invalid model id");Require(c->task=="video-field-v1"||c->task=="audio-field-v1","Unsupported task; arbitrary ONNX interfaces are not accepted");
    Require(j.at("model")=="model.onnx","Model filename must be model.onnx");
    const auto hash=j.at("sha256").get<std::string>();Require(QRegularExpression("^[a-fA-F0-9]{64}$").match(QString::fromStdString(hash)).hasMatch(),"Invalid model SHA256");
    Require(j.at("inputs").is_array()&&j.at("outputs").is_array(),"Tensor lists must be arrays");
    for(const auto& s:j.at("inputs"))c->inputs.push_back(Spec(s));for(const auto& s:j.at("outputs"))c->outputs.push_back(Spec(s));
    std::set<std::string> in,out;std::size_t total=0;
    for(const auto& s:c->inputs){Require(in.insert(s.name).second,"Duplicate input");total+=Count(s.shape);}
    Require(total<=MaxElements,"Combined input limit exceeded");total=0;
    for(const auto& s:c->outputs){Require(out.insert(s.name).second,"Duplicate output");total+=Count(s.shape);}
    Require(total<=MaxElements,"Combined output limit exceeded");
    if(c->task=="video-field-v1")
    {
        Require(in==std::set<std::string>{"current_rgb","previous_rgb","screen_rect","delta_seconds"},"Wrong video-field-v1 input names");
        const auto shape=Named(c->inputs,"current_rgb").shape;
        Require(shape.size()==4&&shape[0]==1&&shape[1]==3&&shape[2]<=256&&shape[3]<=256,"Video shape must be NCHW float32 RGB, maximum 256 by 256");
        Require(Named(c->inputs,"previous_rgb").shape==shape,"Previous image shape differs");
        Require(Named(c->inputs,"screen_rect").shape==std::vector<std::int64_t>{1,4}&&Named(c->inputs,"delta_seconds").shape==std::vector<std::int64_t>{1},"Wrong video geometry/time shapes");
    }
    else
    {
        Require(in==std::set<std::string>{"audio_pcm"},"Wrong audio-field-v1 inputs");const auto s=c->inputs.front().shape;
        Require(s.size()==3&&s[0]==1&&s[1]==1&&s[2]>=480&&s[2]<=240000&&j.at("sample_rate")==48000,"Audio must be mono 48 kHz, 480 through 240000 causal samples");
    }
    Require(out==std::set<std::string>{"field_rgb"}||out==std::set<std::string>{"field_rgb","confidence"},"Wrong field output names");
    const auto field=Named(c->outputs,"field_rgb").shape;
    Require(field.size()==4&&field[0]==1&&field[1]==3&&field[2]<=256&&field[3]<=256,"Field must be linear NCHW RGB, maximum 256 by 256");
    if(out.count("confidence"))Require(Named(c->outputs,"confidence").shape==std::vector<std::int64_t>{1,1,field[2],field[3]},"Wrong confidence shape");
    c->field_rect=j.at("field_rect").get<std::array<double,4>>();
    for(double x:c->field_rect)Require(std::isfinite(x)&&std::abs(x)<=16,"Invalid field rectangle");
    Require(c->field_rect[2]>0&&c->field_rect[3]>0,"Nonpositive field extent");
    Require(j.at("max_age_ms").is_number_integer(),"Result lease must be an integer");c->max_age_ms=j.at("max_age_ms").get<int>();Require(c->max_age_ms>=20&&c->max_age_ms<=2000,"Result lease must be 20 through 2000 ms");
    const auto states=j.value("states",Json::array());Require(states.is_array()&&states.size()<=8,"Too many recurrent state tensors");total=0;
    for(const auto& s:states)
    {
        Require(s.is_object()&&s.size()==3,"State specification needs input, output and shape");
        Require(s.at("shape").is_array()&&std::all_of(s.at("shape").begin(),s.at("shape").end(),[](const auto& d){return d.is_number_integer();}),"State dimensions must be integers");
        StateSpec state{s.at("input").get<std::string>(),s.at("output").get<std::string>(),s.at("shape").get<std::vector<std::int64_t>>()};
        Require(Name(state.input)&&Name(state.output)&&in.insert(state.input).second&&out.insert(state.output).second,"Duplicate or invalid state name");
        total+=Count(state.shape);Require(total<=256*1024,"Recurrent state exceeds 1 MiB");c->states.push_back(std::move(state));
    }
    return c;
}
struct Runtime
{
#ifdef _WIN32
    HMODULE library=nullptr;
#endif
    const OrtApi* api=nullptr;OrtEnv* env=nullptr;OrtSession* session=nullptr;
    OrtMemoryInfo* memory=nullptr;std::string version;
    void Check(OrtStatus* s)const{if(s){std::string e=api->GetErrorMessage(s);api->ReleaseStatus(s);throw std::runtime_error(e);}}
    ~Runtime()
    {
        if(api){if(session)api->ReleaseSession(session);if(memory)api->ReleaseMemoryInfo(memory);if(env)api->ReleaseEnv(env);}
#ifdef _WIN32
        if(library)FreeLibrary(library);
#endif
    }
    void Open(const std::string& filename)
    {
#ifdef _WIN32
        QFileInfo file(QString::fromStdString(filename));Require(file.isAbsolute()&&file.isFile()&&file.fileName().compare("onnxruntime.dll",Qt::CaseInsensitive)==0,"ORT must be an explicit absolute onnxruntime.dll path");
        const auto path=file.canonicalFilePath().toStdWString();library=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);Require(library!=nullptr,"Cannot load ONNX Runtime CPU DLL or its system dependencies");
        using Entry=const OrtApiBase*(ORT_API_CALL*)();const auto get=reinterpret_cast<Entry>(GetProcAddress(library,"OrtGetApiBase"));Require(get!=nullptr,"ORT entry point missing");
        const auto* base=get();api=base->GetApi(ORT_API_VERSION);Require(api!=nullptr,"ORT C API 30 is required");version=base->GetVersionString();
        Check(api->CreateEnv(ORT_LOGGING_LEVEL_ERROR,"OpenRGBIntelligence",&env));Check(api->CreateCpuMemoryInfo(OrtArenaAllocator,OrtMemTypeDefault,&memory));
#else
        (void)filename;throw std::runtime_error("This initial runtime adapter is Windows x64 CPU only");
#endif
    }
    void ValidateSession(const ModelConfig& c)
    {
        std::vector<TensorSpec> expected_in=c.inputs,expected_out=c.outputs;
        for(const auto& s:c.states){expected_in.push_back({s.input,s.shape});expected_out.push_back({s.output,s.shape});}
        OrtAllocator* allocator=nullptr;Check(api->GetAllocatorWithDefaultOptions(&allocator));
        for(bool input:{true,false})
        {
            const auto& expected=input?expected_in:expected_out;std::size_t count=0;Check(input?api->SessionGetInputCount(session,&count):api->SessionGetOutputCount(session,&count));Require(count==expected.size(),"Graph tensor count differs from manifest");
            std::set<std::string> found;
            for(std::size_t i=0;i<count;++i)
            {
                char* raw=nullptr;Check(input?api->SessionGetInputName(session,i,allocator,&raw):api->SessionGetOutputName(session,i,allocator,&raw));
                const std::string name=raw;allocator->Free(allocator,raw);const auto& spec=Named(expected,name);Require(found.insert(name).second,"Duplicate graph name");
                OrtTypeInfo* type=nullptr;Check(input?api->SessionGetInputTypeInfo(session,i,&type):api->SessionGetOutputTypeInfo(session,i,&type));
                try
                {
                    const OrtTensorTypeAndShapeInfo* tensor=nullptr;Check(api->CastTypeInfoToTensorInfo(type,&tensor));Require(tensor!=nullptr,"Only tensor inputs and outputs are supported");
                    ONNXTensorElementDataType element;Check(api->GetTensorElementType(tensor,&element));Require(element==ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,"Only float32 tensors are supported");
                    std::size_t rank=0;Check(api->GetDimensionsCount(tensor,&rank));Require(rank==spec.shape.size(),"Graph rank differs from manifest");std::vector<std::int64_t> shape(rank);Check(api->GetDimensions(tensor,shape.data(),rank));Require(shape==spec.shape,"Graph static dimensions differ from manifest");
                }catch(...){api->ReleaseTypeInfo(type);throw;}api->ReleaseTypeInfo(type);
            }
        }
    }
};
}
double Now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
bool Result::Usable(double now)const{return std::isfinite(now)&&now>=source_time&&now<expires;}
std::shared_ptr<const ModelConfig> ReadManifest(const std::string& path)
{const auto bytes=Read(QString::fromStdString(path),65536);return Parse(Json::parse(bytes.constData(),bytes.constData()+bytes.size()));}

struct Worker::Impl
{
    mutable std::mutex mutex;std::condition_variable wake;std::thread thread;
    bool shutdown=false,active=false,reset_pending=true;std::uint64_t generation=0,latest_epoch=0,latest_sequence=0,minimum_result_sequence=0;double latest_time=-1;
    struct Command{std::uint64_t generation;std::string runtime,path;};std::optional<Command> command;std::optional<Request> pending;
    struct Prepared{std::uint64_t generation;std::function<std::optional<Request>(const ModelConfig&)> callback;};std::optional<Prepared> prepared;
    std::shared_ptr<const ModelConfig> config;std::shared_ptr<const Result> result;StatusSnapshot status;
    const OrtApi* cancel_api=nullptr;OrtRunOptions* run_options=nullptr;OrtSessionOptions* load_options=nullptr;
    bool ValidateLocked(const Request& r);
    void CancelLocked()
    {
        if(cancel_api&&run_options){auto* s=cancel_api->RunOptionsSetTerminate(run_options);if(s)cancel_api->ReleaseStatus(s);}
        if(cancel_api&&load_options){auto* s=cancel_api->SessionOptionsSetLoadCancellationFlag(load_options,true);if(s)cancel_api->ReleaseStatus(s);}
    }
    bool Current(std::uint64_t gen)const{return active&&!shutdown&&generation==gen;}
    void Loop()
    {
        std::unique_ptr<Runtime> runtime;std::shared_ptr<const ModelConfig> model;
        std::vector<Tensor> state;std::uint64_t state_epoch=0;double state_time=-1;
        while(true)
        {
            std::optional<Command> next;std::optional<Request> request;std::optional<Prepared> preparation;
            {
                std::unique_lock<std::mutex> lock(mutex);wake.wait(lock,[&]{return shutdown||command||pending||prepared||(!active&&runtime);});
                if(shutdown)break;
                if(command){next=std::move(command);command.reset();}
                else if(!active){lock.unlock();runtime.reset();model.reset();state.clear();continue;}
                else if(prepared){preparation=std::move(prepared);prepared.reset();}
                else{request=std::move(pending);pending.reset();}
            }
            if(next)
            {
                runtime.reset();model.reset();state.clear();state_epoch=0;state_time=-1;
                try
                {
                    auto candidate=ReadManifest(next->path);const QFileInfo manifest(QString::fromStdString(next->path));
                    QFileInfo model_file(manifest.absoluteDir().filePath("model.onnx"));
                    Require(model_file.canonicalPath()==manifest.canonicalPath(),"Model must remain inside its package directory");
                    auto bytes=Read(model_file.absoluteFilePath(),MaxModelBytes);const auto actual=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex().toStdString();
                    auto wanted=candidate->manifest.at("sha256").get<std::string>();std::transform(wanted.begin(),wanted.end(),wanted.begin(),[](unsigned char x){return char(std::tolower(x));});Require(actual==wanted,"Model SHA256 mismatch");
                    guard::Validate(std::string_view(bytes.constData(),std::size_t(bytes.size())));
                    runtime=std::make_unique<Runtime>();runtime->Open(next->runtime);OrtSessionOptions* options=nullptr;runtime->Check(runtime->api->CreateSessionOptions(&options));
                    try
                    {
                        runtime->Check(runtime->api->SetSessionExecutionMode(options,ORT_SEQUENTIAL));runtime->Check(runtime->api->SetIntraOpNumThreads(options,1));runtime->Check(runtime->api->SetInterOpNumThreads(options,1));
                        runtime->Check(runtime->api->SetSessionGraphOptimizationLevel(options,ORT_ENABLE_BASIC));
                        runtime->Check(runtime->api->AddSessionConfigEntry(options,"session.intra_op.allow_spinning","0"));runtime->Check(runtime->api->AddSessionConfigEntry(options,"session.inter_op.allow_spinning","0"));
                        {std::lock_guard<std::mutex> lock(mutex);Require(Current(next->generation),"Configuration superseded");cancel_api=runtime->api;load_options=options;}
                        // Array loading gives the graph no model directory for external weights.
                        runtime->Check(runtime->api->CreateSessionFromArray(runtime->env,bytes.constData(),std::size_t(bytes.size()),options,&runtime->session));
                        {std::lock_guard<std::mutex> lock(mutex);load_options=nullptr;cancel_api=nullptr;}
                    }
                    catch(...){std::lock_guard<std::mutex> lock(mutex);load_options=nullptr;cancel_api=nullptr;runtime->api->ReleaseSessionOptions(options);throw;}
                    runtime->api->ReleaseSessionOptions(options);runtime->ValidateSession(*candidate);
                    bool keep=false;{std::lock_guard<std::mutex> lock(mutex);keep=Current(next->generation);if(keep){model=candidate;config=candidate;status.state="ready";status.detail="ONNX CPU session ready; no claim of learned-model quality";status.runtime_version=runtime->version;}}if(!keep)runtime.reset();
                }
                catch(const std::exception& e)
                {
                    runtime.reset();std::lock_guard<std::mutex> lock(mutex);if(Current(next->generation)){config.reset();status.state="error";status.detail=e.what();}
                }
                continue;
            }
            if(preparation&&model)
            {
                try
                {
                    {std::lock_guard<std::mutex> lock(mutex);if(!Current(preparation->generation))continue;}
                    request=preparation->callback(*model);
                    std::lock_guard<std::mutex> lock(mutex);
                    if(!Current(preparation->generation))continue;
                    if(!request){status.state="waiting-input";status.detail="Waiting for a complete causal input";continue;}
                    if(request->generation!=preparation->generation||!ValidateLocked(*request)){request.reset();continue;}
                }
                catch(const std::exception& e){std::lock_guard<std::mutex> lock(mutex);if(Current(preparation->generation)){++status.rejected;status.state="ready";status.detail=e.what();}continue;}
            }
            if(!request||!runtime||!model)continue;
            const auto& req=*request;
            bool reset=false;
            {
                std::lock_guard<std::mutex> lock(mutex);
                if(!Current(req.generation)||req.epoch!=latest_epoch){++status.stale;continue;}
                if(Now()-req.source_time>=model->max_age_ms/1000.){++status.stale;reset_pending=true;continue;}
                reset=reset_pending||req.reset||state_epoch!=req.epoch||state_time<0||req.source_time-state_time>.3;reset_pending=false;
                status.state="running";
            }
            if(reset){state.clear();for(const auto& s:model->states){Tensor t;t.name=s.input;t.shape=s.shape;t.values.resize(Count(s.shape));state.push_back(std::move(t));}}
            std::vector<OrtValue*> ins,outs;std::vector<const OrtValue*> const_ins;std::vector<const char*> in_names,out_names;OrtRunOptions* options=nullptr;
            const double started=Now();bool accepted=false;
            try
            {
                auto append=[&](const Tensor& t){OrtValue* v=nullptr;runtime->Check(runtime->api->CreateTensorWithDataAsOrtValue(runtime->memory,const_cast<float*>(t.values.data()),t.values.size()*sizeof(float),t.shape.data(),t.shape.size(),ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,&v));ins.push_back(v);const_ins.push_back(v);in_names.push_back(t.name.c_str());};
                for(const auto& t:req.inputs)append(t);for(const auto& t:state)append(t);
                std::vector<TensorSpec> all_outputs=model->outputs;for(const auto& s:model->states)all_outputs.push_back({s.output,s.shape});
                for(const auto& s:all_outputs)out_names.push_back(s.name.c_str());outs.resize(all_outputs.size());
                runtime->Check(runtime->api->CreateRunOptions(&options));
                {std::lock_guard<std::mutex> lock(mutex);Require(Current(req.generation),"Inference superseded");cancel_api=runtime->api;run_options=options;}
                runtime->Check(runtime->api->Run(runtime->session,options,in_names.data(),const_ins.data(),ins.size(),out_names.data(),out_names.size(),outs.data()));
                auto published=std::make_shared<Result>();published->generation=req.generation;published->epoch=req.epoch;published->sequence=req.sequence;published->source_time=req.source_time;published->completed_time=Now();published->expires=req.source_time+model->max_age_ms/1000.;
                std::vector<Tensor> next_state;
                for(std::size_t i=0;i<outs.size();++i)
                {
                    OrtTensorTypeAndShapeInfo* shape=nullptr;runtime->Check(runtime->api->GetTensorTypeAndShape(outs[i],&shape));std::size_t count=0;try{runtime->Check(runtime->api->GetTensorShapeElementCount(shape,&count));std::size_t rank=0;runtime->Check(runtime->api->GetDimensionsCount(shape,&rank));Require(rank==all_outputs[i].shape.size(),"Runtime output rank changed");std::vector<std::int64_t> dims(rank);runtime->Check(runtime->api->GetDimensions(shape,dims.data(),rank));Require(dims==all_outputs[i].shape,"Runtime output dimensions changed");}catch(...){runtime->api->ReleaseTensorTypeAndShapeInfo(shape);throw;}runtime->api->ReleaseTensorTypeAndShapeInfo(shape);Require(count==Count(all_outputs[i].shape),"Runtime output size differs from static contract");
                    float* data=nullptr;runtime->Check(runtime->api->GetTensorMutableData(outs[i],reinterpret_cast<void**>(&data)));Tensor t;t.name=all_outputs[i].name;t.shape=all_outputs[i].shape;t.values.assign(data,data+count);Require(Finite(t.values),"Non-finite model output rejected");
                    if(i<model->outputs.size()){Require(std::all_of(t.values.begin(),t.values.end(),[](float x){return x>=0&&x<=1;}),"Field/confidence output outside [0,1]");published->outputs.push_back(std::move(t));}
                    else{t.name=model->states[i-model->outputs.size()].input;next_state.push_back(std::move(t));}
                }
                std::lock_guard<std::mutex> lock(mutex);
                if(Current(req.generation)&&req.epoch==latest_epoch&&req.sequence>=minimum_result_sequence&&published->Usable(Now())){result=published;state=std::move(next_state);state_epoch=req.epoch;state_time=req.source_time;++status.completed;status.state="ready";status.detail="Native ONNX inference completed";accepted=true;}
                else{++status.stale;if(Current(req.generation)){status.state="ready";status.detail="Late or obsolete inference discarded";}}
            }
            catch(const std::exception& e)
            {std::lock_guard<std::mutex> lock(mutex);if(Current(req.generation)){++status.rejected;status.state="ready";status.detail=e.what();}}
            {
                std::lock_guard<std::mutex> lock(mutex);run_options=nullptr;cancel_api=nullptr;status.last_run_ms=(Now()-started)*1000;
            }
            for(auto* v:ins)runtime->api->ReleaseValue(v);for(auto* v:outs)if(v)runtime->api->ReleaseValue(v);if(options)runtime->api->ReleaseRunOptions(options);
            if(!accepted){state_epoch=0;state_time=-1;state.clear();}
        }
    }
};
Worker::Worker():impl(std::make_unique<Impl>()){}
Worker::~Worker(){RequestStop();{std::lock_guard<std::mutex> lock(impl->mutex);impl->shutdown=true;}impl->wake.notify_one();if(impl->thread.joinable())impl->thread.join();}
std::uint64_t Worker::Configure(const std::string& dll,const std::string& path)
{
    std::lock_guard<std::mutex> lock(impl->mutex);impl->CancelLocked();const auto generation=++impl->generation;impl->active=true;impl->config.reset();impl->result.reset();impl->pending.reset();impl->prepared.reset();impl->latest_epoch=impl->latest_sequence=0;impl->latest_time=-1;impl->command=Impl::Command{generation,dll,path};impl->status.state="loading";impl->status.detail.clear();impl->status.generation=generation;if(!impl->thread.joinable())impl->thread=std::thread([p=impl.get()]{p->Loop();});impl->wake.notify_one();return generation;
}
bool Worker::Impl::ValidateLocked(const Request& r)
{
    try
    {
        Require(Current(r.generation)&&bool(config),"Session is unavailable or request generation is obsolete");const auto& c=*config;
        const double now=Now();Require(std::isfinite(r.source_time)&&r.source_time<=now&&now-r.source_time<c.max_age_ms/1000.,"Request timestamp is invalid or stale");
        Require(r.epoch>0&&r.sequence>0&&r.epoch>=latest_epoch,"Invalid or obsolete stream epoch");
        Require(r.epoch!=latest_epoch||(r.sequence>latest_sequence&&r.source_time>latest_time),"Duplicate or reordered sequence/time");Require(r.inputs.size()==c.inputs.size(),"Wrong input tensor count");std::set<std::string> names;
        for(const auto& t:r.inputs){Require(names.insert(t.name).second,"Duplicate request tensor");ValidateTensor(t,Named(c.inputs,t.name));}
        if(c.task=="video-field-v1")
        {
            const auto& dt=*std::find_if(r.inputs.begin(),r.inputs.end(),[](const auto& t){return t.name=="delta_seconds";});Require(dt.values[0]>=0&&dt.values[0]<=.3f,"Invalid causal delta_seconds");
            for(const auto& t:r.inputs)if(t.name=="current_rgb"||t.name=="previous_rgb")Require(std::all_of(t.values.begin(),t.values.end(),[](float x){return x>=0&&x<=1;}),"RGB input outside [0,1]");
            const auto& rect=*std::find_if(r.inputs.begin(),r.inputs.end(),[](const auto& t){return t.name=="screen_rect";});Require(rect.values[2]>0&&rect.values[3]>0&&std::all_of(rect.values.begin(),rect.values.end(),[](float x){return std::abs(x)<=16;}),"Invalid screen rectangle");
        }
        else Require(std::all_of(r.inputs[0].values.begin(),r.inputs[0].values.end(),[](float x){return x>=-1&&x<=1;}),"PCM input outside [-1,1]");
        if(r.reset||r.epoch!=latest_epoch){result.reset();reset_pending=true;minimum_result_sequence=r.sequence;}
        latest_epoch=r.epoch;latest_sequence=r.sequence;latest_time=r.source_time;return true;
    }
    catch(const std::exception& e){++status.rejected;status.detail=e.what();if(Current(r.generation))reset_pending=true;return false;}
}
bool Worker::Submit(Request r){std::lock_guard<std::mutex> lock(impl->mutex);if(!impl->ValidateLocked(r))return false;if(impl->pending||impl->prepared)++impl->status.replaced;impl->prepared.reset();impl->pending=std::move(r);impl->wake.notify_one();return true;}
bool Worker::SubmitPrepared(std::uint64_t generation,std::function<std::optional<Request>(const ModelConfig&)> prepare)
{std::lock_guard<std::mutex> lock(impl->mutex);if(!prepare||!impl->Current(generation)||!impl->config)return false;if(impl->pending||impl->prepared)++impl->status.replaced;impl->pending.reset();impl->prepared=Impl::Prepared{generation,std::move(prepare)};impl->wake.notify_one();return true;}
std::shared_ptr<const ModelConfig> Worker::Config()const{std::lock_guard<std::mutex> lock(impl->mutex);return impl->config;}
std::shared_ptr<const Result> Worker::Latest()const{std::lock_guard<std::mutex> lock(impl->mutex);return impl->result;}
StatusSnapshot Worker::Status()const{std::lock_guard<std::mutex> lock(impl->mutex);return impl->status;}
void Worker::RequestStop(){std::lock_guard<std::mutex> lock(impl->mutex);impl->CancelLocked();++impl->generation;impl->active=false;impl->command.reset();impl->pending.reset();impl->prepared.reset();impl->config.reset();impl->result.reset();impl->status.state="stopped";impl->status.detail.clear();impl->status.generation=impl->generation;impl->wake.notify_one();}
}
