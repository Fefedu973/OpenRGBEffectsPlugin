// SPDX-License-Identifier: GPL-2.0-or-later
#include "ScreenSourceSelection.h"
#include "QtScreenCapturer.h"
#ifdef _WIN32
#include "WindowsScreenCapturer.h"
#endif
#include <QGuiApplication>
#include <QScreen>
#include <QFormLayout>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include <QTimer>
#include <QUuid>

namespace
{
bool SceneReady(const std::shared_ptr<better_source::Discovery>& connection,const QString& owner,
                const std::shared_ptr<const better_source::FrameSnapshot>& image,bool requested)
{
    if(!image || !image->Usable() || !connection->Read()->Ready() || connection->Read()->instance_id!=image->instance_id)return false;
    const auto control=connection->ReadControl(owner);
    if(control->phase==better_source::ControlPhase::Idle)return !requested;
    return control->phase==better_source::ControlPhase::Effective && control->instance_id==image->instance_id &&
        control->scene_id==image->scene_id && image->scene_revision==control->scene_revision && image->control_revision==control->control_revision;
}
}

ScreenSourceSelection::ScreenSourceSelection(QWidget* parent,bool offerAppearance):QGroupBox(tr("Screen source"),parent),owner(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    auto* form=new QFormLayout(this);
    kind=new QComboBox(this);kind->setObjectName("screen_source_kind");
    kind->addItem(tr("Native display"),"native");kind->addItem(tr("BetterScreenCapture — automatic connection"),"better");
    kind->addItem(tr("Shared memory channel (advanced)"),"surface");
    display=new QComboBox(this);display->setObjectName("screen_source_display");
    for(auto* screen:QGuiApplication::screens()) display->addItem(screen->name());
    channel=new QLineEdit("better-screen-capture",this);channel->setObjectName("screen_source_channel");
    channel->setMaxLength(64);channel->setValidator(new QRegularExpressionValidator(QRegularExpression("[A-Za-z0-9_-]{1,64}"),channel));
    descriptor=new QLineEdit(this);descriptor->setObjectName("screen_source_descriptor");
    descriptor->setPlaceholderText(tr("Automatic — current Better profile"));
    scene=new QComboBox(this);scene->setObjectName("screen_source_scene");scene->addItem(tr("Follow active Better scene"),QString());
    status=new QLabel(this);status->setWordWrap(true);
    form->addRow(tr("Source"),kind);form->addRow(tr("Display"),display);form->addRow(tr("Connection file (optional)"),descriptor);form->addRow(tr("Frame channel"),channel);form->addRow(status);
    form->insertRow(3,tr("Scene while this effect runs"),scene);
    if(offerAppearance)
    {
        follow_appearance=new QCheckBox(tr("Follow Better appearance (native placement, filters and glow)"),this);
        follow_appearance->setObjectName("screen_source_follow_appearance");follow_appearance->setChecked(true);
        form->insertRow(4,follow_appearance);
        connect(follow_appearance,&QCheckBox::toggled,this,[this]{Reconfigure();});
    }
    status->setTextFormat(Qt::PlainText);
#ifdef _WIN32
    capturer=std::make_unique<WindowsScreenCapturer>();
#else
    capturer=std::make_unique<QtScreenCapturer>();
#endif
    capturer->SetFrameRate(60);
    connect(capturer.get(),&ScreenCapturer::OnImage,this,[this](const QImage& image){
        if(image.isNull()) return;
        std::lock_guard<std::mutex> guard(mutex);
        if(!native_active) return;
        auto frame=std::make_shared<DynamicShaderImage>();frame->image=image;
        frame->generation=generation;frame->sequence=++sequence;
        frame->source_revision=generation;
        native=std::move(frame);native_status=tr("Native capture active");
    },Qt::DirectConnection);
    connect(capturer.get(),&ScreenCapturer::OnError,this,[this](const ScreenCapturerError&,const QString& message){
        std::lock_guard<std::mutex> guard(mutex);native.reset();native_status=message;
    },Qt::DirectConnection);
    connect(kind,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{Reconfigure();});
    connect(display,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{Reconfigure();});
    connect(channel,&QLineEdit::editingFinished,this,[this]{Reconfigure();});
    connect(descriptor,&QLineEdit::editingFinished,this,[this]{Reconfigure();});
    connect(scene,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{requested_scene=scene->currentData().toString();Reconfigure();});
    auto* timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{if(running||isVisible())RefreshStatus();});timer->start(500);
    Reconfigure();
}
ScreenSourceSelection::~ScreenSourceSelection(){destroying=true;SetRunning(false);}
void ScreenSourceSelection::SetRunning(bool value){running=value;Reconfigure();}
void ScreenSourceSelection::Reconfigure()
{
    {std::lock_guard<std::mutex> guard(mutex);native_active=false;native.reset();}
    capturer->Stop();
    std::shared_ptr<screen_source::Source> old;
    std::shared_ptr<better_source::Discovery> old_discovery;
    std::shared_ptr<better_source::FrameSource> old_paired;
    std::shared_ptr<better_source::AppearanceInput> old_appearance;
    {std::lock_guard<std::mutex> guard(mutex);old.swap(external);old_discovery.swap(discovery);old_paired.swap(paired);old_appearance.swap(appearance);appearance_active=false;++generation;sequence=0;}
    if(old_discovery)old_discovery->ReleaseScene(owner);
    old_appearance.reset();old_paired.reset();old.reset();old_discovery.reset();
    bound_channel.clear();bound_instance.clear();claimed_scene.clear();
    const bool better=kind->currentData().toString()=="better";
    const bool manual=kind->currentData().toString()=="surface";
    source_error.clear();
    channel->setEnabled(manual);display->setEnabled(!better && !manual);descriptor->setEnabled(better);
    scene->setEnabled(better);if(follow_appearance)follow_appearance->setEnabled(better);
    {std::lock_guard<std::mutex> guard(mutex);appearance_active=better&&follow_appearance&&follow_appearance->isChecked();scene_required=better&&!requested_scene.isEmpty();scene_invalid=false;}
    if(better)RefreshBetter();
    else if(running && manual && channel->hasAcceptableInput())
    {
        screen_source::Config config;config.channel=channel->text().toStdString();
        try
        {
            auto input=screen_source::Source::Acquire(config);
            std::lock_guard<std::mutex> guard(mutex);external=std::move(input);
        }
        catch(const std::exception& error){source_error=tr("Unable to start frame input: %1").arg(QString::fromUtf8(error.what()));}
    }
    else if(running && !manual && display->currentIndex()>=0)
    {
        int current_index=-1;const auto screens=QGuiApplication::screens();
        for(int i=0;i<screens.size();++i)if(screens[i]->name()==display->currentText()){current_index=i;break;}
        if(current_index<0)source_error=tr("Selected display is disconnected.");
        else
        {
            capturer->SetScreen(current_index);
            {std::lock_guard<std::mutex> guard(mutex);native_active=true;native_status=tr("Waiting for native capture");}
            capturer->Start();
        }
    }
    RefreshStatus();
}
void ScreenSourceSelection::RefreshBetter()
{
    if(destroying || kind->currentData().toString()!="better" || (!running && !isVisible()))return;
    if(!discovery)
    {
        try {better_source::Config config;config.descriptor_path=descriptor->text();auto next=better_source::Discovery::Acquire(config);std::lock_guard<std::mutex> lock(mutex);discovery=std::move(next);}
        catch(const std::exception&){source_error=tr("Unable to start Better connection discovery.");return;}
    }
    const auto connection=discovery->Read();
    if(connection->Ready())
    {
        bool changed=scene->count()!=connection->scenes.size()+1;
        if(!changed)for(int i=0;i<connection->scenes.size();++i)if(scene->itemData(i+1)!=connection->scenes[i].id || scene->itemText(i+1)!=connection->scenes[i].name){changed=true;break;}
        if(changed)
        {
            const QSignalBlocker block(scene);scene->clear();scene->addItem(tr("Follow active Better scene"),QString());
            for(const auto& entry:connection->scenes)scene->addItem(entry.name,entry.id);
        }
        const QSignalBlocker block(scene);
        if(!requested_scene.isEmpty()&&scene->findData(requested_scene)<0)scene->addItem(tr("Saved scene (currently unavailable)"),requested_scene);
        scene->setCurrentIndex(std::max(0,scene->findData(requested_scene)));
    }
    if(running && claimed_scene!=requested_scene)
    {
        const auto result=discovery->RequestScene(owner,requested_scene);claimed_scene=requested_scene;
        {std::lock_guard<std::mutex> lock(mutex);scene_invalid=result==better_source::RequestResult::Invalid;}
        if(scene_invalid)source_error=tr("The saved Better scene identifier is invalid; select a valid scene.");
    }
    if(!connection->Ready() || !running)
    {
        std::shared_ptr<screen_source::Source> old;
        {std::lock_guard<std::mutex> guard(mutex);if(external){old.swap(external);++generation;}}
        bound_channel.clear();bound_instance.clear();
        return;
    }
    if(bound_channel==connection->raw_channel && bound_instance==connection->instance_id)return;
    try
    {
        screen_source::Config config;config.channel=connection->raw_channel.toStdString();config.ttl_ms=connection->recommended_ttl_ms;
        auto next=screen_source::Source::Acquire(config);
        std::shared_ptr<better_source::FrameSource> next_paired;
        std::shared_ptr<better_source::AppearanceInput> next_appearance;
        if(appearance_active || !requested_scene.isEmpty())next_paired=better_source::FrameSource::Acquire(discovery);
        if(appearance_active)next_appearance=std::make_shared<better_source::AppearanceInput>(next_paired,output_width,output_height);
        std::shared_ptr<screen_source::Source> old;
        std::shared_ptr<better_source::FrameSource> old_paired;
        std::shared_ptr<better_source::AppearanceInput> old_appearance;
        {std::lock_guard<std::mutex> guard(mutex);old.swap(external);external=std::move(next);old_paired.swap(paired);paired=std::move(next_paired);old_appearance.swap(appearance);appearance=std::move(next_appearance);++generation;}
        bound_channel=connection->raw_channel;bound_instance=connection->instance_id;channel->setText(bound_channel);
        if(!scene_invalid)source_error.clear();
    }
    catch(const std::exception&){source_error=tr("Better advertised an unsupported frame output.");}
}
std::shared_ptr<const DynamicShaderImage> ScreenSourceSelection::Latest() const
{
    std::shared_ptr<screen_source::Source> source;
    std::shared_ptr<better_source::FrameSource> metadata;
    std::shared_ptr<better_source::Discovery> connection;
    bool requested;
    std::uint64_t revision;
    {std::lock_guard<std::mutex> guard(mutex);if(native_active)return native;if(scene_invalid)return {};source=external;revision=generation;metadata=paired;connection=discovery;requested=scene_required;}
    if(metadata && connection)
    {
        const auto bound=metadata->Read();if(!SceneReady(connection,owner,bound,requested))return {};
        auto frame=std::make_shared<DynamicShaderImage>();frame->image=bound->raw;
        frame->generation=bound->raw_generation;frame->sequence=bound->raw_sequence;frame->expires=bound->expires;frame->source_revision=revision;
        frame->metadata_generation=true;
        {std::lock_guard<std::mutex> lock(mutex);if(generation!=revision || paired!=metadata || discovery!=connection)return {};}
        return frame;
    }
    if(!source) return {};
    const auto snapshot=source->Read();
    if(!snapshot.Usable() || !snapshot.frame) return {};
    {std::lock_guard<std::mutex> guard(mutex);if(generation!=revision || external!=source)return {};}
    auto frame=std::make_shared<DynamicShaderImage>();frame->image=snapshot.frame->image;
    frame->generation=snapshot.frame->generation;frame->sequence=snapshot.frame->sequence;
    frame->source_revision=revision;
    frame->metadata_generation=bool(connection);
    frame->expires=snapshot.frame->expires;
    return frame;
}
std::shared_ptr<const ShaderRenderGraphFrame> ScreenSourceSelection::LatestAppearance() const
{
    std::shared_ptr<better_source::AppearanceInput> input;std::shared_ptr<better_source::FrameSource> metadata;std::shared_ptr<better_source::Discovery> connection;
    std::uint64_t revision;bool requested,invalid;
    {std::lock_guard<std::mutex> lock(mutex);if(!appearance_active)return {};input=appearance;metadata=paired;connection=discovery;revision=generation;requested=scene_required;invalid=scene_invalid;}
    const auto black=[](){return std::make_shared<const ShaderRenderGraphFrame>();};
    if(!input || !metadata || !connection || invalid)return black();
    const auto frame=input->Read();const auto bound=metadata->Read();
    if(!SceneReady(connection,owner,bound,requested) || !frame->images.count("raw") || frame->images.at("raw")->generation!=bound->raw_generation)return black();
    {std::lock_guard<std::mutex> lock(mutex);if(generation!=revision || paired!=metadata || discovery!=connection || appearance!=input)return black();}
    return frame;
}
void ScreenSourceSelection::SetOutputSize(unsigned width,unsigned height)
{
    output_width=width;output_height=height;
    std::shared_ptr<better_source::AppearanceInput> input;{std::lock_guard<std::mutex> lock(mutex);input=appearance;}
    if(input)input->Resize(width,height);
}
void ScreenSourceSelection::RefreshStatus()
{
    RefreshBetter();
    if(scene_invalid){status->setText(source_error);return;}
    if(kind->currentData().toString()=="better" && discovery)
    {
        const auto connection=discovery->Read();
        if(!connection->Ready()){status->setText(connection->detail);return;}
        const auto control=discovery->ReadControl(owner);
        if(control->phase!=better_source::ControlPhase::Idle && control->phase!=better_source::ControlPhase::Effective){status->setText(control->detail);return;}
        if(appearance){status->setText(appearance->Status());return;}
    }
    if(!running){status->setText(tr("Capture starts with this effect."));return;}
    std::shared_ptr<screen_source::Source> source;
    {std::lock_guard<std::mutex> guard(mutex);if(native_active){status->setText(native_status);return;}source=external;}
    if(!source){status->setText(source_error.isEmpty()?tr("Choose a valid source/channel."):source_error);return;}
    const auto snapshot=source->Read();
    QString description=QString::fromUtf8(screen_source::StateName(snapshot.state));
    if(snapshot.frame)description+=QString(" — %1 × %2").arg(snapshot.frame->image.width()).arg(snapshot.frame->image.height());
    if(!snapshot.detail.empty())description+=" — "+QString::fromStdString(snapshot.detail);
    status->setText(description);
}
nlohmann::json ScreenSourceSelection::Save() const
{
    return {{"kind",kind->currentData().toString().toStdString()},{"display",display->currentIndex()},
            {"display_name",display->currentText().toStdString()},{"channel",channel->text().toStdString()},
            {"connection_file",descriptor->text().toStdString()}, {"scene",requested_scene.toStdString()},
            {"follow_better_appearance",follow_appearance&&follow_appearance->isChecked()}};
}
void ScreenSourceSelection::Load(const nlohmann::json& settings)
{
    if(!settings.is_object())return;
    const QSignalBlocker a(kind),b(display),c(channel),d(descriptor);
    if(settings.contains("kind") && settings["kind"].is_string())kind->setCurrentIndex(settings["kind"]=="better"?1:settings["kind"]=="surface"?2:0);
    if(settings.contains("connection_file") && settings["connection_file"].is_string())descriptor->setText(QString::fromStdString(settings["connection_file"]));
    if(settings.contains("scene") && settings["scene"].is_string())requested_scene=QString::fromStdString(settings["scene"]);
    if(follow_appearance && settings.contains("follow_better_appearance") && settings["follow_better_appearance"].is_boolean())
    {const QSignalBlocker block(follow_appearance);follow_appearance->setChecked(settings["follow_better_appearance"].get<bool>());}
    if(settings.contains("display") && settings["display"].is_number_integer())display->setCurrentIndex(std::clamp(settings["display"].get<int>(),0,std::max(0,display->count()-1)));
    if(settings.contains("display_name") && settings["display_name"].is_string())
    {const auto i=display->findText(QString::fromStdString(settings["display_name"]));if(i>=0)display->setCurrentIndex(i);}
    if(settings.contains("channel") && settings["channel"].is_string())
    {const auto text=QString::fromStdString(settings["channel"]);if(QRegularExpression("^[A-Za-z0-9_-]{1,64}$").match(text).hasMatch())channel->setText(text);}
    Reconfigure();
}
