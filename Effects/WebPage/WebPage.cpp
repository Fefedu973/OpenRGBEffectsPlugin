/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "WebPage.h"
#include <QFormLayout>
#include <QPushButton>
#include <QMetaObject>
#include <QSignalBlocker>
#include <QDebug>

REGISTER_EFFECT(WebPage);

WebPage::WebPage(QWidget* parent):RGBEffect(parent),capture(new WebPageCapture(this))
{
    EffectDetails.EffectClassName=ClassName();
    EffectDetails.EffectName=tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription=tr("Renders a web page to a canvas, image outputs and LED samples.").toStdString();
    EffectDetails.HasCustomSettings=true;
    EffectDetails.SupportsRandom=false;
    auto* form=new QFormLayout(this);
    url_edit=new QLineEdit(this); url_edit->setPlaceholderText("https://example.org/effect.html or file:///C:/effects/page.html");
    form->addRow(tr("Page URL"),url_edit);
    width_edit=new QSpinBox(this); width_edit->setRange(16,4096); width_edit->setValue(width);
    height_edit=new QSpinBox(this); height_edit->setRange(16,4096); height_edit->setValue(height);
    fps_edit=new QSpinBox(this); fps_edit->setRange(1,30); fps_edit->setValue(fps);
    form->addRow(tr("Canvas width"),width_edit); form->addRow(tr("Canvas height"),height_edit); form->addRow(tr("Capture FPS limit"),fps_edit);
    publish_edit=new QCheckBox(tr("Publish an external FrameSurface"),this);
    channel_edit=new QLineEdit(QString::fromStdString(channel),this);
    form->addRow(publish_edit); form->addRow(tr("FrameSurface channel"),channel_edit);
    auto* apply=new QPushButton(tr("Apply / reload page"),this); form->addRow(apply);
    status=new QLabel(tr("Stopped. The browser starts only while this effect is enabled."),this); status->setWordWrap(true); form->addRow(status);
    connect(apply,&QPushButton::clicked,this,&WebPage::Apply);
    connect(capture,&WebPageCapture::Status,status,&QLabel::setText);
    connect(capture,&WebPageCapture::FrameReady,this,[this](const QImage& frame)
    {
        std::lock_guard<std::mutex> guard(mutex);
        if(!running || frame.size()!=QSize(width,height)) return;
        image=frame; ++sequence; received=std::chrono::steady_clock::now();
    });
#ifndef WEBPAGE_FRAME_SURFACE
    publish_edit->setEnabled(false);
#endif
}

WebPage::~WebPage()
{
    EffectState(false);
    capture->Stop();
}

void WebPage::Apply()
{
    const QUrl candidate(url_edit->text().trimmed());
    const auto next_channel=channel_edit->text().toStdString();
    const bool valid_channel=!next_channel.empty() && next_channel.size()<=64 && std::all_of(next_channel.begin(),next_channel.end(),[](unsigned char c)
    {return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_';});
    if(!WebPageCapture::ValidUrl(candidate) || !valid_channel)
    { status->setText(tr("Enter an absolute http/https/local file URL and a channel of 1–64 letters, digits, underscores or hyphens.")); return; }
    {
        std::lock_guard<std::mutex> guard(mutex);
        url=candidate.toString(); width=width_edit->value(); height=height_edit->value(); fps=fps_edit->value();
        publish=publish_edit->isChecked(); channel=next_channel; image={}; ++revision;
#ifdef WEBPAGE_FRAME_SURFACE
        publisher.reset();
#endif
    }
    RestartBrowser();
}

void WebPage::RestartBrowser()
{
    // Can be requested by the effect worker; browser creation stays on GUI/STA.
    QMetaObject::invokeMethod(this,[this]
    {
        QUrl page; int w,h,rate; bool enabled;
        { std::lock_guard<std::mutex> guard(mutex); page=QUrl(url); w=width; h=height; rate=fps; enabled=running; }
        capture->Stop();
        if(enabled) capture->Start(page,w,h,rate);
        else status->setText(tr("Stopped; browser resources released."));
    },Qt::QueuedConnection);
}

void WebPage::EffectState(bool enabled)
{
    {
        std::lock_guard<std::mutex> guard(mutex);
        EffectEnabled=enabled; running=enabled; image={};
        router.SetRunning(enabled);
#ifdef WEBPAGE_FRAME_SURFACE
        publisher.reset();
#endif
    }
    RestartBrowser();
}

void WebPage::OnControllerZonesListChanged(std::vector<ControllerZone*>)
{ std::lock_guard<std::mutex> guard(mutex); ++revision; }

void WebPage::StepEffect(std::vector<ControllerZone*> zones)
{
    std::lock_guard<std::mutex> guard(mutex);
    const auto now=std::chrono::steady_clock::now();
    if(!running || image.isNull() || now-received>std::chrono::seconds(2)) return;
    router.frame.Update(image,sequence,Brightness,Temperature,Tint);
#ifdef WEBPAGE_FRAME_SURFACE
    if(publish)
    {
        if(!publisher || channel!=publisher_channel)
        { publisher.reset(new room_surface::Publisher(channel)); publisher_channel=channel; published_sequence=0; }
        if(publisher->IsOpen() && (published_sequence!=router.frame.sequence || now-last_publication>=std::chrono::milliseconds(500)))
        {
            const auto& frame=router.frame;
            if(frame.pixels && publisher->PublishBGRA(frame.pixels->data(),frame.pixels->size(),frame.width,frame.height,frame.stride))
            {published_sequence=frame.sequence; last_publication=now;}
        }
    }
    else publisher.reset();
#endif
    for(auto* zone:zones)
    {
        const auto region=effect_canvas::RegionFor(zone,regions);
        if(router.Route(zone,image,sequence,Brightness,Temperature,Tint,region)) continue;
        for(const auto& sample:led_plans.Get(zone,revision,region))
        {
            const auto pixel=effect_canvas::SamplePixel(image,sample);
            zone->SetLED(sample.led,ToRGBColor(qRed(pixel),qGreen(pixel),qBlue(pixel)),Brightness,Temperature,Tint);
        }
    }
}

void WebPage::LoadCustomSettings(json settings)
{
    if(!settings.is_object()) return;
    if(settings.contains("url") && settings["url"].is_string()) url_edit->setText(QString::fromStdString(settings["url"]));
    auto number=[&](const char* key,int fallback,int minimum,int maximum)
    {if(!settings.contains(key)||!settings[key].is_number_integer()) return fallback; const auto n=settings[key].get<std::int64_t>();return int(std::clamp<std::int64_t>(n,minimum,maximum));};
    width_edit->setValue(number("width",800,16,4096)); height_edit->setValue(number("height",600,16,4096)); fps_edit->setValue(number("fps",20,1,30));
    if(settings.contains("frame_channel")&&settings["frame_channel"].is_string()) channel_edit->setText(QString::fromStdString(settings["frame_channel"]));
    publish_edit->setChecked(settings.contains("publish_frame")&&settings["publish_frame"].is_boolean()&&settings["publish_frame"].get<bool>());
    {std::lock_guard<std::mutex> guard(mutex); regions=effect_canvas::ValidRegions(settings.value("zone_regions",json::array())); ++revision;}
    Apply();
}

json WebPage::SaveCustomSettings()
{
    std::lock_guard<std::mutex> guard(mutex);
    return {{"url",url.toStdString()},{"width",width},{"height",height},{"fps",fps},{"publish_frame",publish},{"frame_channel",channel},{"zone_regions",regions}};
}
