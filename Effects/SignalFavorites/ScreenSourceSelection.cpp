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

ScreenSourceSelection::ScreenSourceSelection(QWidget* parent):QGroupBox(tr("Screen source"),parent)
{
    auto* form=new QFormLayout(this);
    kind=new QComboBox(this);kind->setObjectName("screen_source_kind");
    kind->addItem(tr("Native display"),"native");kind->addItem(tr("BetterScreenCapture — native frames"),"better");
    display=new QComboBox(this);display->setObjectName("screen_source_display");
    for(auto* screen:QGuiApplication::screens()) display->addItem(screen->name());
    channel=new QLineEdit("better-screen-capture",this);channel->setObjectName("screen_source_channel");
    channel->setMaxLength(64);channel->setValidator(new QRegularExpressionValidator(QRegularExpression("[A-Za-z0-9_-]{1,64}"),channel));
    status=new QLabel(this);status->setWordWrap(true);
    form->addRow(tr("Source"),kind);form->addRow(tr("Display"),display);form->addRow(tr("Frame channel"),channel);form->addRow(status);
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
    auto* timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{if(isVisible())RefreshStatus();});timer->start(500);
    Reconfigure();
}
ScreenSourceSelection::~ScreenSourceSelection(){SetRunning(false);}
void ScreenSourceSelection::SetRunning(bool value){running=value;Reconfigure();}
void ScreenSourceSelection::Reconfigure()
{
    {std::lock_guard<std::mutex> guard(mutex);native_active=false;native.reset();}
    capturer->Stop();
    std::shared_ptr<screen_source::Source> old;
    {std::lock_guard<std::mutex> guard(mutex);old.swap(external);++generation;sequence=0;}
    old.reset();
    const bool better=kind->currentData().toString()=="better";
    source_error.clear();
    channel->setEnabled(better);display->setEnabled(!better);
    if(running && better && channel->hasAcceptableInput())
    {
        screen_source::Config config;config.channel=channel->text().toStdString();
        try
        {
            auto input=screen_source::Source::Acquire(config);
            std::lock_guard<std::mutex> guard(mutex);external=std::move(input);
        }
        catch(const std::exception& error){source_error=tr("Unable to start frame input: %1").arg(QString::fromUtf8(error.what()));}
    }
    else if(running && !better && display->currentIndex()>=0)
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
std::shared_ptr<const DynamicShaderImage> ScreenSourceSelection::Latest() const
{
    std::shared_ptr<screen_source::Source> source;
    std::uint64_t revision;
    {std::lock_guard<std::mutex> guard(mutex);if(native_active)return native;source=external;revision=generation;}
    if(!source) return {};
    const auto snapshot=source->Read();
    if(!snapshot.Usable() || !snapshot.frame) return {};
    {std::lock_guard<std::mutex> guard(mutex);if(generation!=revision || external!=source)return {};}
    auto frame=std::make_shared<DynamicShaderImage>();frame->image=snapshot.frame->image;
    frame->generation=snapshot.frame->generation;frame->sequence=snapshot.frame->sequence;
    frame->source_revision=revision;
    frame->expires=snapshot.frame->expires;
    return frame;
}
void ScreenSourceSelection::RefreshStatus()
{
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
            {"display_name",display->currentText().toStdString()},{"channel",channel->text().toStdString()}};
}
void ScreenSourceSelection::Load(const nlohmann::json& settings)
{
    if(!settings.is_object())return;
    const QSignalBlocker a(kind),b(display),c(channel);
    if(settings.contains("kind") && settings["kind"].is_string())kind->setCurrentIndex(settings["kind"]=="better"?1:0);
    if(settings.contains("display") && settings["display"].is_number_integer())display->setCurrentIndex(std::clamp(settings["display"].get<int>(),0,std::max(0,display->count()-1)));
    if(settings.contains("display_name") && settings["display_name"].is_string())
    {const auto i=display->findText(QString::fromStdString(settings["display_name"]));if(i>=0)display->setCurrentIndex(i);}
    if(settings.contains("channel") && settings["channel"].is_string())
    {const auto text=QString::fromStdString(settings["channel"]);if(QRegularExpression("^[A-Za-z0-9_-]{1,64}$").match(text).hasMatch())channel->setText(text);}
    Reconfigure();
}
