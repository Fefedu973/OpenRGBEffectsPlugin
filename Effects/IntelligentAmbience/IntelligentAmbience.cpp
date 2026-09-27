// SPDX-License-Identifier: GPL-2.0-or-later
#include "IntelligentAmbience.h"
#include "ShaderCanvas.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <QTimer>
#include <QCoreApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QPushButton>
#include "Audio/AudioManager.h"
#include <chrono>
#include <cmath>
REGISTER_EFFECT(IntelligentAmbience);
namespace
{
double Now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
std::string RuntimePath()
{
    const auto test_path=qEnvironmentVariable("OPENRGB_INTELLIGENCE_ORT");
    return (test_path.isEmpty()?QCoreApplication::applicationDirPath()+"/plugins/IntelligentAmbience/onnxruntime.dll":test_path).toStdString();
}
void Hide(QLayout* layout){while(auto* item=layout->takeAt(0)){if(item->widget())item->widget()->hide();if(item->layout())Hide(item->layout());delete item;}}
std::string Shader(const char* name){QFile file(QString(":/shaders/IntelligentAmbience/")+name);if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Missing Intelligent Ambience shader");return file.readAll().toStdString();}
double Number(const json& j,const char* key,double fallback,double low,double high){const auto value=j.value(key,json(fallback));if(!value.is_number())return fallback;const double x=value.get<double>();return std::isfinite(x)?std::clamp(x,low,high):fallback;}
room_ai::VideoFrame FromImage(const QImage& raw,double now,std::uint64_t sequence,std::uint64_t epoch)
{
    // Bounded nearest-neighbour input avoids averaging encoded sRGB values.
    // The analysis engine subsequently pools decoded samples in linear light.
    const auto scaled=raw.scaled(192,192,Qt::KeepAspectRatio,Qt::FastTransformation).convertToFormat(QImage::Format_RGB888);
    room_ai::VideoFrame frame;frame.width=scaled.width();frame.height=scaled.height();frame.stamp={epoch,1,0,sequence,now};
    auto data=std::make_shared<std::vector<std::uint8_t>>(frame.width*frame.height*3);
    for(unsigned y=0;y<frame.height;++y)std::copy_n(scaled.constScanLine(int(y)),frame.width*3,data->data()+y*frame.width*3);
    frame.rgb=std::move(data);return frame;
}
room_ai::VideoFrame Demonstration(double now,double origin,std::uint64_t seq,std::uint64_t epoch)
{
    room_ai::VideoFrame f;f.width=160;f.height=90;f.stamp={epoch,1,0,seq,now};
    auto pixels=std::make_shared<std::vector<std::uint8_t>>(160*90*3);const double cx=.1+std::fmod(now-origin,5)*.36;
    for(unsigned y=0;y<90;++y)for(unsigned x=0;x<160;++x){const double dx=x/160.-cx,dy=y/160.-.28125;const float a=float(std::exp(-(dx*dx+dy*dy)/.003));const auto i=(y*160+x)*3;(*pixels)[i]=std::uint8_t(room_ai::Encode(.01f+.95f*a)*255);(*pixels)[i+1]=std::uint8_t(room_ai::Encode(.02f+.2f*a)*255);(*pixels)[i+2]=std::uint8_t(room_ai::Encode(.035f+.03f*a)*255);}
    f.rgb=std::move(pixels);return f;
}
}
IntelligentAmbience::IntelligentAmbience(QWidget* parent):Shaders(parent)
{
    EffectDetails.EffectName=UI_Name();EffectDetails.EffectClassName=ClassName();EffectDetails.MinSpeed=EffectDetails.MaxSpeed=0;
    EffectDetails.EffectDescription="Native screen ambience and musical composition using the existing Visual Map canvas. Optional local ONNX model packs; procedural rendering is the default.";
    EffectDetails.ExpandCustomSettings=true;
    Hide(layout());delete layout();auto* outer=new QVBoxLayout(this);auto* form=new QFormLayout;outer->addLayout(form);
    mode=new QComboBox(this);mode->setObjectName("ia_mode");mode->addItems({tr("Video ambience"),tr("Music accompaniment"),tr("Hybrid")});form->addRow(tr("Mode"),mode);
    predictive=new QCheckBox(tr("Predictive / directed rendering (off = classical reference)"),this);predictive->setObjectName("ia_predictive");predictive->setChecked(true);form->addRow(predictive);
    auto number=[&](const char* name,double low,double high,double value){auto* p=new QDoubleSpinBox(this);p->setObjectName(name);p->setRange(low,high);p->setDecimals(2);p->setSingleStep(.05);p->setValue(value);return p;};
    persistence=number("ia_persistence",.1,2.5,1);strength=number("ia_strength",0,1,.75);hybrid=number("ia_hybrid",0,1,.25);
    form->addRow(tr("Peripheral memory (seconds)"),persistence);form->addRow(tr("Prediction strength"),strength);form->addRow(tr("Music contribution in hybrid mode"),hybrid);
    auto* geometry=new QGroupBox(tr("Screen position in the existing 2D canvas"),this);auto* grid=new QFormLayout(geometry);
    const char* names[]={"ia_screen_x","ia_screen_y","ia_screen_w","ia_screen_h"};const char* labels[]={"X","Y","Width","Height"};
    for(unsigned i=0;i<4;++i){screen_fields[i]=number(names[i],i<2?-640:1,i<2?640:1280,parameters.screen[i]);screen_fields[i]->setSingleStep(1);grid->addRow(tr(labels[i]),screen_fields[i]);}
    auto* hint=new QLabel(tr("Coordinates use a 320 × 200 reference canvas. Set the rectangle occupied by the screen; Visual Map keeps all device placements. Defaults are illustrative, not a calibration of your room."),this);hint->setWordWrap(true);grid->addRow(hint);outer->addWidget(geometry);
    source=new ScreenSourceSelection(this,false);source->Load({{"kind","better"}});source->SetOutputSize(800,500);outer->addWidget(source);
    demo=new QCheckBox(tr("Synthetic sources (no screen or audio capture)"),this);demo->setObjectName("ia_demo");outer->addWidget(demo);
    auto* model_group=new QGroupBox(tr("Optional local models"),this);auto* model_form=new QFormLayout(model_group);
    const char* model_names[]={"video","music"};
    for(unsigned i=0;i<2;++i)
    {
        const auto prefix=QString("ia_model_")+model_names[i];
        model_checks[i]=new QCheckBox(i==0?tr("Use video model"):tr("Use music model"),this);model_checks[i]->setObjectName(prefix+"_enabled");
        model_form->addRow(model_checks[i]);
        auto* row=new QWidget(this);auto* line=new QHBoxLayout(row);line->setContentsMargins(0,0,0,0);
        model_paths[i]=new QLineEdit(this);model_paths[i]->setObjectName(prefix+"_path");model_paths[i]->setPlaceholderText(tr("Local model package manifest (.json)"));line->addWidget(model_paths[i]);
        auto* browse=new QPushButton(tr("Browse…"),this);line->addWidget(browse);
        auto* reload=new QPushButton(tr("Reload"),this);reload->setObjectName(prefix+"_reload");line->addWidget(reload);model_form->addRow(row);
        model_status[i]=new QLabel(tr("Disabled — procedural rendering"),this);model_status[i]->setObjectName(prefix+"_status");model_status[i]->setWordWrap(true);model_form->addRow(model_status[i]);
        connect(model_checks[i],&QCheckBox::toggled,this,[this]{ControlsChanged();});
        connect(model_paths[i],&QLineEdit::editingFinished,this,[this]{ControlsChanged();});
        connect(browse,&QPushButton::clicked,this,[this,i]{const auto path=QFileDialog::getOpenFileName(this,tr("Select local model package"),model_paths[i]->text(),tr("Model manifest (*.json)"));if(!path.isEmpty()){model_paths[i]->setText(path);ControlsChanged();}});
        connect(reload,&QPushButton::clicked,this,[this,i]{{std::lock_guard<std::mutex> guard(state_mutex);models[i].configured_key.clear();}ConfigureModels();});
    }
    inference_fps=number("ia_inference_fps",1,30,10);inference_fps->setDecimals(0);inference_fps->setSingleStep(1);model_form->addRow(tr("Maximum model updates / second"),inference_fps);
    connect(inference_fps,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{ControlsChanged();});
    auto* model_hint=new QLabel(tr("Models run locally in OpenRGB. Select a compatible video-field-v1 or audio-field-v1 package; arbitrary .pt/.onnx files need an adapter. Missing, late or invalid model results use procedural rendering."),this);model_hint->setWordWrap(true);model_form->addRow(model_hint);outer->addWidget(model_group);
    ShaderUi()->use_audio->show();ShaderUi()->use_audio->setText(tr("Use the existing Effects audio capture"));outer->addWidget(ShaderUi()->use_audio);outer->addWidget(ShaderUi()->audio_settings);
    connect(CaptureSettings(),&AudioSettings::AudioDeviceChanged,this,[this](int device){{std::lock_guard<std::mutex> guard(state_mutex);++audio_epoch;current_audio_device=device;music.Reset();models[1].configured_key.clear();inputs[3].reset();}ConfigureModels();});
    ShaderUi()->show_rendering->setText(tr("Preview the existing Visual Map canvas"));ShaderUi()->show_rendering->show();outer->addWidget(ShaderUi()->show_rendering);outer->addWidget(ShaderUi()->preview);
    ShaderUi()->preview->setMinimumSize(320,200);ShaderUi()->preview->hide();
    status=new QLabel(tr("Prototype ready. Geometric prediction; no learned model loaded."),this);status->setObjectName("ia_status");status->setWordWrap(true);outer->addWidget(status);
    connect(mode,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{ControlsChanged();});
    for(auto* check:{demo,predictive})connect(check,&QCheckBox::toggled,this,[this]{ControlsChanged();});
    for(auto* field:{persistence,strength,hybrid,screen_fields[0],screen_fields[1],screen_fields[2],screen_fields[3]})connect(field,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{ControlsChanged();});
    auto* timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{if(!isVisible())return;std::lock_guard<std::mutex> guard(state_mutex);status->setText(status_text);for(unsigned i=0;i<2;++i){const auto s=models[i].worker.Status();model_status[i]->setText(!parameters.model_enabled[i]?tr("Disabled — procedural rendering"):QString::fromStdString(models[i].adaptation_error.empty()?s.state+": "+s.detail:models[i].adaptation_error)+tr(" | %1 ms, %2 completed, %3 stale").arg(s.last_run_ms,0,'f',1).arg(qulonglong(s.completed)).arg(qulonglong(s.stale)));}});timer->start(250);
    InstallProgram();Shaders::LoadCustomSettings({{"width",800},{"height",500},{"use_audio",false},{"rhythm_tracking",true}});SetFPS(60);origin=Now();StepEffect({});
}
IntelligentAmbience::~IntelligentAmbience(){EffectState(false);}
void IntelligentAmbience::InstallProgram()
{
    auto* program=Renderer()->Program();program->SetVersion("130");program->Resize(800,500);
    program->main_pass->data.fragment_shader=Shader("video.fs")+"\n"+Shader("music.fs")+R"GLSL(
uniform float iaMode,iaHybrid;
uniform vec4 iImageAvailable,iaVideoFieldRect,iaMusicFieldRect;
uniform float iaVideoModel,iaMusicModel;
vec4 iaModelField(sampler2D source,vec2 point,vec4 bounds)
{
    vec2 uv=(point-bounds.xy)/bounds.zw;
    if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))return vec4(0);
    return clamp(texture(source,uv),0.0,1.0);
}
void mainImage(out vec4 color,in vec2 pixel)
{
    vec2 canvas=vec2(pixel.x/iResolution.x,1.0-pixel.y/iResolution.y)*vec2(320,200);
    vec2 world=(canvas-iaScreen.xy)/iaScreen.z;
    vec3 video=(iaMode<.5||iaMode>1.5)&&iScreenAvailable>.5 ? iaVideoLinear(world) : vec3(0);
    vec3 music=iaMode>.5 ? iaMusicSample(world) : vec3(0);
    if(iaVideoModel>.5&&iImageAvailable.z>.5&&iaValid>.5&&iaOutside(world)>0.0)
    {vec4 field=iaModelField(iChannel2,world,iaVideoFieldRect);video=mix(video,field.rgb,field.a*iaStrength);}
    if(iaMusicModel>.5&&iImageAvailable.w>.5)
    {vec4 field=iaModelField(iChannel3,canvas/vec2(320,200),iaMusicFieldRect);music=mix(music,field.rgb,field.a);}
    if(iaMode<.5)color=vec4(iaLinearToSrgb(video),1);
    else if(iaMode<1.5)color=vec4(iaLinearToSrgb(music),1);
    else color=vec4(iaLinearToSrgb(clamp(video+music*iaHybrid,0.0,1.0)),1);
}
)GLSL";
    for(unsigned i=0;i<4;++i){auto* pass=new ShaderPass(ShaderPass::DYNAMIC_IMAGE);pass->data.image_slot=i;program->passes.push_back(pass);}program->recompile=true;
}
IntelligentAmbience::Parameters IntelligentAmbience::Parse(const json& value)
{
    Parameters p;if(!value.is_object())return p;
    p.mode=int(Number(value,"mode",0,0,2));p.demo=value.contains("demo")&&value["demo"].is_boolean()?value["demo"].get<bool>():false;
    p.predictive=value.contains("predictive")&&value["predictive"].is_boolean()?value["predictive"].get<bool>():true;
    p.persistence=Number(value,"persistence",1,.1,2.5);p.strength=Number(value,"strength",.75,0,1);p.hybrid=Number(value,"hybrid",.25,0,1);
    p.inference_fps=Number(value,"inference_fps",10,1,30);
    const char* model_keys[]={"video_model","music_model"};
    for(unsigned i=0;i<2;++i)if(value.contains(model_keys[i])&&value[model_keys[i]].is_object())
    {
        const auto& model=value[model_keys[i]];
        p.model_enabled[i]=model.contains("enabled")&&model["enabled"].is_boolean()&&model["enabled"].get<bool>();
        if(model.contains("manifest")&&model["manifest"].is_string())
        {const auto path=model["manifest"].get<std::string>();if(path.size()<=4096&&path.find('\0')==std::string::npos)p.model_path[i]=path;}
    }
    if(value.contains("screen")&&value["screen"].is_array()&&value["screen"].size()==4)
    {for(unsigned i=0;i<4;++i){const auto& item=value["screen"][i];if(item.is_number()){const double x=item.get<double>();if(std::isfinite(x))p.screen[i]=std::clamp(x,i<2?-640.:1.,i<2?640.:1280.);}}}
    // The world model supports screen-height/width ratios from .1 through4.
    p.screen[3]=std::clamp(p.screen[3],p.screen[2]*.1,p.screen[2]*4);return p;
}
json IntelligentAmbience::Serialize(const Parameters& p){return {{"mode",p.mode},{"demo",p.demo},{"predictive",p.predictive},{"persistence",p.persistence},{"strength",p.strength},{"hybrid",p.hybrid},{"screen",p.screen},{"inference_fps",p.inference_fps},{"video_model",{{"enabled",p.model_enabled[0]},{"manifest",p.model_path[0]}}},{"music_model",{{"enabled",p.model_enabled[1]},{"manifest",p.model_path[1]}}}};}
void IntelligentAmbience::SyncControls()
{
    Parameters p;{std::lock_guard<std::mutex> guard(state_mutex);p=parameters;}
    const QSignalBlocker a(mode),b(demo),c(predictive),d(persistence),e(strength),f(hybrid);mode->setCurrentIndex(p.mode);demo->setChecked(p.demo);predictive->setChecked(p.predictive);persistence->setValue(p.persistence);strength->setValue(p.strength);hybrid->setValue(p.hybrid);
    for(unsigned i=0;i<4;++i){const QSignalBlocker block(screen_fields[i]);screen_fields[i]->setValue(p.screen[i]);}
    for(unsigned i=0;i<2;++i){const QSignalBlocker a(model_checks[i]),b(model_paths[i]);model_checks[i]->setChecked(p.model_enabled[i]);model_paths[i]->setText(QString::fromStdString(p.model_path[i]));}
    const QSignalBlocker model_rate(inference_fps);inference_fps->setValue(p.inference_fps);
}
void IntelligentAmbience::ControlsChanged()
{
    Parameters p;p.mode=mode->currentIndex();p.demo=demo->isChecked();p.predictive=predictive->isChecked();p.persistence=persistence->value();p.strength=strength->value();p.hybrid=hybrid->value();for(unsigned i=0;i<4;++i)p.screen[i]=screen_fields[i]->value();for(unsigned i=0;i<2;++i){p.model_enabled[i]=model_checks[i]->isChecked();p.model_path[i]=model_paths[i]->text().toStdString();}p.inference_fps=inference_fps->value();p=Parse(Serialize(p));
    {std::lock_guard<std::mutex> guard(state_mutex);if(parameters.demo!=p.demo||parameters.mode!=p.mode){video.Reset();music.Reset();previous_source.reset();previous_render.reset();inputs={};last_analysis=-1;missing_since=-1;++stream_epoch;++texture_generation;}if(parameters.screen!=p.screen){++stream_epoch;inputs[2].reset();models[0].result.reset();}parameters=p;}
    SyncControls();ConfigureCapture();ConfigureModels();
}
void IntelligentAmbience::ConfigureCapture()
{
    Parameters p;bool running;{std::lock_guard<std::mutex> guard(state_mutex);p=parameters;running=enabled;}
    source->setEnabled(p.mode!=1&&!p.demo);
    const bool desired=running&&p.mode!=1&&!p.demo;
    if(source_running!=desired){source_running=desired;source->SetRunning(desired);}
    ShaderUi()->use_audio->setChecked(p.mode!=0&&!p.demo);ShaderUi()->audio_settings->setVisible(p.mode!=0&&!p.demo);
    ShaderUi()->use_audio->setEnabled(false); // Mode owns capture; demo must never open audio.
    {std::lock_guard<std::mutex> guard(state_mutex);current_audio_device=CaptureDeviceIndex();}
}
void IntelligentAmbience::ConfigureModels()
{
    const auto runtime=RuntimePath();
    std::lock_guard<std::mutex> guard(state_mutex);
    for(unsigned i=0;i<2;++i)
    {
        auto& slot=models[i];const auto& path=parameters.model_path[i];
        const bool wanted=enabled&&parameters.predictive&&parameters.model_enabled[i]&&!path.empty()&&
            (i==0?parameters.mode!=1:parameters.mode!=0);
        const auto key=wanted?runtime+"\n"+path+"\n"+(parameters.demo?"demo":"live"):std::string();
        if(!wanted)
        {if(!slot.configured_key.empty())slot.worker.RequestStop();slot.configured_key.clear();slot.result.reset();slot.adaptation_error.clear();inputs[i+2].reset();continue;}
        if(slot.configured_key==key)continue;
        slot.configured_key=key;slot.last_submit=-1;slot.sequence=0;slot.result.reset();slot.adaptation_error.clear();inputs[i+2].reset();
        slot.video_adapter=std::make_shared<room_ai::inference::VideoAdapter>();
        slot.audio_adapter=std::make_shared<room_ai::inference::AudioAdapter>();
        slot.audio_input_epoch=std::make_shared<std::atomic<std::uint64_t>>(0);
        slot.generation=slot.worker.Configure(runtime,path);
    }
}
void IntelligentAmbience::UpdateModels(const std::shared_ptr<const DynamicShaderImage>& frame,
                                      double now,bool video_valid,ShaderUniformMap& values)
{
    // Caller owns state_mutex. Preparation owns its inputs and runs on the model
    // worker; no callback retains this widget or touches a capture/GL resource.
    using namespace room_ai::inference;
    const auto& p=parameters;
    const auto render=video.RenderState();
    const bool cut=video.Stats().cut;
    if(cut&&!previous_model_cut){++stream_epoch;inputs[2].reset();models[0].result.reset();}
    previous_model_cut=cut;
    const std::array<double,4> normalized{p.screen[0]/320.,p.screen[1]/200.,p.screen[2]/320.,p.screen[3]/200.};
    for(unsigned i=0;i<2;++i)
    {
        auto& slot=models[i];
        const auto config=slot.worker.Config();
        const bool wanted=enabled&&p.predictive&&p.model_enabled[i]&&!slot.configured_key.empty()&&
            (i==0?p.mode!=1&&video_valid:p.mode!=0);
        const bool compatible=config&&config->task==(i==0?"video-field-v1":"audio-field-v1");
        if(config&&!compatible)slot.adaptation_error="Wrong model task for this input; procedural rendering";
        else slot.adaptation_error.clear();
        if(wanted&&compatible&&(slot.last_submit<0||now-slot.last_submit>=1./p.inference_fps))
        {
            slot.last_submit=now;const auto generation=slot.generation;
            if(i==0)
            {
                const auto image=p.demo?demo_image:(frame?frame->image:QImage());
                const auto adapter=slot.video_adapter;const auto epoch=stream_epoch,sequence=++slot.sequence;
                slot.worker.SubmitPrepared(generation,[image,adapter,generation,epoch,sequence,now,normalized](const ModelConfig& model)
                {return adapter->Make(model,image,generation,epoch,sequence,now,normalized);});
            }
            else
            {
                const auto adapter=slot.audio_adapter;const auto device=current_audio_device;const auto input_epoch=slot.audio_input_epoch;
                const auto synthetic=p.demo;const auto sequence=++slot.sequence,epoch=audio_epoch.load();
                slot.worker.SubmitPrepared(generation,[adapter,device,synthetic,sequence,epoch,generation,input_epoch](const ModelConfig& model)->std::optional<Request>
                {
                    const auto count=AudioSamples(model);if(!count)return {};
                    room_audio::PcmWindowSnapshot pcm;
                    if(synthetic)
                    {
                        pcm.status=room_audio::PcmWindowStatus::Ready;pcm.epoch=epoch;pcm.sequence=sequence;
                        pcm.source_rate=48000;pcm.source_channels=1;pcm.source_end=room_ai::inference::Now();
                        pcm.source_begin=pcm.source_end-double(count)/48000.;pcm.captured_through=pcm.source_end;
                        pcm.mono.resize(count);
                        for(std::size_t n=0;n<count;++n)pcm.mono[n]=float(.2*std::sin(6.283185307179586*440*(pcm.source_begin+double(n)/48000.)));
                    }
                    else pcm=AudioManager::get()->CapturePcmWindow(device,count,room_ai::inference::Now());
                    input_epoch->store(pcm.Ready()?pcm.epoch:0);
                    return adapter->Make(model,pcm,generation,room_ai::inference::Now());
                });
            }
        }
        const auto result=slot.worker.Latest();
        const bool valid=wanted&&compatible&&result&&result->generation==slot.generation&&result->Usable(now)&&
            (i==0?(result->epoch==stream_epoch&&render&&!render->suppress_prediction):
                  (slot.audio_input_epoch&&slot.audio_input_epoch->load()==result->epoch));
        if(valid)
        {
            if(result!=slot.result)
            {inputs[i+2]=FieldImage(*result,*config,room_ai::inference::Now());if(inputs[i+2])slot.result=result;}
        }
        else {inputs[i+2].reset();slot.result.reset();}
        const auto rect=config?config->field_rect:std::array<double,4>{0,0,1,1};
        values[i==0?"iaVideoFieldRect":"iaMusicFieldRect"]={{{float(rect[0]),float(rect[1]),float(rect[2]),float(rect[3])}},4};
        values[i==0?"iaVideoModel":"iaMusicModel"]={{{inputs[i+2]?1.f:0.f,0,0,0}},1};
    }
}
void IntelligentAmbience::EffectState(bool active)
{
    {std::lock_guard<std::mutex> guard(state_mutex);enabled=active;video.Reset();music.Reset();previous_source.reset();previous_render.reset();inputs={};++stream_epoch;++texture_generation;observation_sequence=0;last_analysis=-1;origin=Now();missing_since=-1;}
    ConfigureCapture();ConfigureModels();if(active&&!Renderer()->isRunning())StepEffect({});Shaders::EffectState(active);
}
json IntelligentAmbience::SaveCustomSettings()
{
    auto value=Shaders::SaveCustomSettings();value.erase("shader_program");value.erase("shader_name");value["screen_source"]=source->Save();std::lock_guard<std::mutex> guard(state_mutex);value["intelligent"]=Serialize(parameters);value["intelligent_schema"]=1;return value;
}
void IntelligentAmbience::LoadCustomSettings(json value)
{
    if(!value.is_object())return;const auto next=Parse(value.value("intelligent",json::object()));
    value.erase("shader_program");value.erase("shader_name");
    const auto w=unsigned(Number(value,"width",800,1,4096)),h=unsigned(Number(value,"height",500,1,4096));
    value["width"]=ShaderCanvas::ValidSize(w,h)?w:800;value["height"]=ShaderCanvas::ValidSize(w,h)?h:500;
    value["use_audio"]=next.mode!=0&&!next.demo;value["rhythm_tracking"]=true;
    {std::lock_guard<std::mutex> guard(state_mutex);parameters=next;video.Reset();music.Reset();previous_source.reset();previous_render.reset();inputs={};last_analysis=-1;missing_since=-1;++stream_epoch;++texture_generation;}
    if(value.contains("screen_source"))source->Load(value["screen_source"]);SyncControls();Shaders::LoadCustomSettings(value);source->SetOutputSize(value["width"],value["height"]);ConfigureCapture();ConfigureModels();
}

void IntelligentAmbience::StepEffect(std::vector<ControllerZone*> zones)
{
    // Capture publishes its own QPC timestamp. Read it BEFORE the render clock.
    const auto input_epoch=audio_epoch.load();
    const auto audio=CaptureSignalSnapshot();
    const auto frame=source->Latest();
    const double now=Now();
    ShaderUniformMap values;
    std::array<std::shared_ptr<const DynamicShaderImage>,4> published;
    {
        std::lock_guard<std::mutex> guard(state_mutex);
        const auto& p=parameters;
        auto scalar=[&](const std::string& name,double x){values[name]={{float(x),0,0,0},1};};
        auto vec4=[&](const std::string& name,std::array<float,4> x){values[name]={x,4};};
        const bool live=enabled&&p.mode!=1&&!p.demo&&frame&&frame->Usable()&&!frame->image.isNull();
        bool video_valid=false;
        video.SetPersistence(p.persistence);video.SetStrength(p.strength);video.SetScreenHeight(p.screen[3]/p.screen[2]);
        if(enabled&&p.mode!=1&&p.demo)
        {
            if(last_analysis<0||now-last_analysis>=1./30)
            {const auto sample=Demonstration(now,origin,++observation_sequence,stream_epoch);demo_image=QImage(sample.rgb->data(),int(sample.width),int(sample.height),int(sample.width*3),QImage::Format_RGB888).copy();video.Push(sample);last_analysis=now;}
            video_valid=true;
        }
        else if(live)
        {
            missing_since=-1;
            const bool discontinuity=previous_source&&
                (previous_source->source_revision!=frame->source_revision||
                 previous_source->Width()!=frame->Width()||previous_source->Height()!=frame->Height()||
                 (previous_source->generation!=frame->generation&&!frame->metadata_generation));
            if(discontinuity)
            {video.Reset();previous_source.reset();previous_render.reset();inputs={};last_analysis=-1;++stream_epoch;++texture_generation;}
            const auto current_video=video.RenderState();
            const bool changed=!current_video||!current_video->valid||!previous_source||previous_source->sequence!=frame->sequence||
                (previous_source->generation!=frame->generation&&!frame->metadata_generation)||previous_source->image.cacheKey()!=frame->image.cacheKey();
            if(changed&&(last_analysis<0||now-last_analysis>=1./30))
            {
                if(video.Push(FromImage(frame->image,now,++observation_sequence,stream_epoch)))previous_source=frame;
                last_analysis=now;
            }
            else if(!changed)previous_source=frame; // Adopt metadata/lease renewal without inventing a capture.
            // A static producer can renew its lease without sending new pixels.
            // This keeps the base image alive, never extends motion-event memory.
            video.Refresh(now);video_valid=true;
        }
        else if(previous_source)
        {
            if(missing_since<0)missing_since=now;
            if(now-missing_since>.75)
            {video.Reset();previous_source.reset();previous_render.reset();inputs={};last_analysis=-1;++stream_epoch;++texture_generation;}
        }
        const auto render=video.RenderState();
        if(render&&render->valid)
        {
            if(!previous_render||previous_render->revision!=render->revision||
               previous_render->grid.rgba!=render->grid.rgba||previous_render->events.rgba!=render->events.rgba)
            {
                const room_ai::VideoTexture planes[]={render->grid,render->events};
                for(unsigned i=0;i<2;++i)
                {
                    auto image=std::make_shared<DynamicShaderImage>();image->width=planes[i].width;image->height=planes[i].height;
                    image->rgba32f=planes[i].rgba;image->sequence=render->revision;image->generation=texture_generation;inputs[i]=std::move(image);
                }
            }
            previous_render=render;
            if(video_valid)
            {
                auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(750);
                if(live)deadline=std::min(deadline,frame->expires);
                for(unsigned i=0;i<2;++i)if(inputs[i])
                {auto live_image=std::make_shared<DynamicShaderImage>(*inputs[i]);live_image->expires=deadline;inputs[i]=std::move(live_image);}
            }
        }
        scalar("iaValid",video_valid&&render&&render->valid);
        scalar("iaAge",render?now-render->source_time:0);scalar("iaSourceAge",render?now-render->alive_time:0);
        scalar("iaScreenHeight",p.screen[3]/p.screen[2]);scalar("iaPersistence",p.persistence);scalar("iaStrength",p.strength);
        scalar("iaPredictive",p.predictive);scalar("iaSuppress",render&&render->suppress_prediction);scalar("iaEventCount",render?render->event_count:0);
        vec4("iaScreen",{float(p.screen[0]),float(p.screen[1]),float(p.screen[2]),float(p.screen[3])});
        scalar("iaMode",p.mode);scalar("iaHybrid",p.hybrid);
        if(enabled&&p.mode!=0&&!p.demo&&input_epoch==audio_epoch.load())music.Push(audio,now);
        const auto composition=music.RenderState(now);
        values["iaMusicPalette"]={{composition.palette.r,composition.palette.g,composition.palette.b,0},3};
        vec4("iaMusicControls",composition.controls);scalar("iaMusicMotionPhase",composition.motion_phase);scalar("iaMusicDirected",p.predictive);
        vec4("iaMusicBands0",{composition.bands[0],composition.bands[1],composition.bands[2],composition.bands[3]});
        vec4("iaMusicBands1",{composition.bands[4],composition.bands[5],composition.bands[6],composition.bands[7]});
        for(unsigned i=0;i<composition.accents.size();++i)vec4("iaMusicAccents["+std::to_string(i)+"]",composition.accents[i]);
        const auto video_stats=video.Stats();const auto audio_stats=music.Snapshot(now);
        UpdateModels(frame,now,video_valid,values);
        status_text=tr("%1 | Video: %2, %3 tracked events, analysis %4 ms. Music: %5, %6 BPM, confidence %7. Model output: %8.")
            .arg(p.demo?tr("Synthetic test"):tr("Native prototype"))
            .arg(video_valid?QString::fromStdString(video_stats.mode):tr("waiting / inactive"))
            .arg(qulonglong(video_stats.event_count)).arg(video_stats.frame_ms,0,'f',2)
            .arg(QString::fromStdString(audio_stats.scene)).arg(audio_stats.tempo_bpm,0,'f',1).arg(audio_stats.confidence,0,'f',2)
            .arg(inputs[2]&&inputs[3]?tr("video + music"):inputs[2]?tr("video"):inputs[3]?tr("music"):tr("none — procedural rendering"));
        published=inputs;
    }
    Renderer()->UpdateInputs(values,published);
    Shaders::StepEffect(std::move(zones));
}
