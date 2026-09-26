/*---------------------------------------------------------*\
| Ambient.cpp                                               |
|                                                           |
|   OpenRGB Effects Plugin Ambient Effect                   |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "Ambient.h"
#include <QDebug>
#include <QSignalBlocker>
#include "ColorUtils.h"
#include "OpenRGBEffectSettings.h"
#include "QtScreenCapturer.h"

#ifdef __linux__
#include "WaylandScreenCapturer.h"
#elif _WIN32
#include "WindowsScreenCapturer.h"
#endif

REGISTER_EFFECT(Ambient);

Ambient::Ambient(QWidget *parent) :
    RGBEffect(parent),
    ui(new Ui::Ambient)
{
    ui->setupUi(this);

    SetDynamicStrings();
    EffectDetails.EffectClassName   = ClassName();
    EffectDetails.HasCustomSettings = true;
    EffectDetails.SupportsRandom = false;

    ui->select_screen->hide();
    ui->crop_frame->hide();

#ifdef __linux__
    bool isWayland = qgetenv("XDG_SESSION_TYPE") == "wayland";

    if(isWayland)
    {
        capturer = new WaylandScreenCapturer();

        ui->select_rectangle->hide();
        ui->screen_label->hide();
        ui->screen->hide();        
        ui->select_screen->show();
    }
#elif _WIN32
    capturer = new WindowsScreenCapturer();
#endif

    if(capturer == nullptr)
    {
        capturer = new QtScreenCapturer();
    }

    ui->framerate->setValue(OpenRGBEffectSettings::globalSettings.fpscapture);

    rectangle_selector_overlay = new RectangleSelectorOverlay(this);

    connect(rectangle_selector_overlay, &RectangleSelectorOverlay::SelectionUpdated, [=](QRect rect){
        ui->left->setValue(rect.left());
        ui->top->setValue(rect.top());
        ui->width->setValue(rect.width());
        ui->height->setValue(rect.height());
    });

    QList<QScreen*> screens = QGuiApplication::screens();

    for(QScreen* screen: screens)
    {
        ui->screen->addItem(screen->name());
    }

    // The capture worker delivers an owned QImage. A direct connection keeps a
    // single latest frame rather than accumulating queued images on the GUI.
    connect(capturer, &ScreenCapturer::OnImage, this, &Ambient::ReceiveImage, Qt::DirectConnection);

    connect(capturer, &ScreenCapturer::OnError, [&](const ScreenCapturerError& err, const QString& message){
        qDebug() << "ScreenCapturer::OnError" << err << message;
    });

    capturer->SetScreen(0);

    connect(capturer, &ScreenCapturer::OnRestoreTokenProvided, this, [&](const QString token){
        qDebug() << "[Ambient] capturer provided a restore token";
        std::lock_guard<std::mutex> guard(lock);
        restore_token = token;
    });

#ifndef AMBIENT_HAS_FRAME_SURFACE
    ui->publish_frame->setEnabled(false);
    ui->publish_frame->setToolTip(tr("Frame output requires the Windows OpenRGB Room FrameSurface extension."));
#endif
}

Ambient::~Ambient()
{
    capturer->Stop();
    delete capturer;
    delete rectangle_selector_overlay;
    delete ui;
}

void Ambient::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        SetDynamicStrings();
    }
}

void Ambient::SetDynamicStrings()
{
    EffectDetails.EffectName        = tr(UI_Name().c_str()).toStdString();
    EffectDetails.EffectDescription = tr("Takes a portion of the screen and reflect it to your devices").toStdString();
    ui->mode->clear();
    ui->mode->addItems({
                           tr("Scaled average"),
                           tr("Screen copy"),
                       });
}

void Ambient::EffectState(const bool state)
{
    EffectEnabled = state;
    image_router.SetRunning(state);
#ifdef AMBIENT_HAS_FRAME_SURFACE
    {
        std::lock_guard<std::mutex> guard(publication_lock);
        publication_running = state;
        if(!state) { publisher.reset(); published_sequence = 0; }
    }
#endif

    if(state)
    {
        qDebug() << "[Ambient] Start capturer";
        capturer->Start();
    }
    else
    {
        capturer->Stop();
    }
}

void Ambient::ReceiveImage(const QImage& captured)
{
    bool crop;
    QRect rectangle;
    int canvas_width, canvas_height;
    std::uint64_t revision;
    {
        std::lock_guard<std::mutex> guard(lock);
        crop = crop_stream;
        rectangle = QRect(left, top, width, height);
        canvas_width = working_width;
        canvas_height = working_height;
        revision = config_revision;
    }
    QImage normalized = effect_canvas::Normalize(captured, crop, rectangle, canvas_width, canvas_height);
    if(normalized.isNull()) return;
    std::lock_guard<std::mutex> guard(lock);
    if(revision != config_revision) return; // Do not install a frame from obsolete crop/size settings.
    image = std::move(normalized);
    ++image_sequence;
}

void Ambient::OnControllerZonesListChanged(std::vector<ControllerZone*>)
{
    std::lock_guard<std::mutex> guard(lock);
    ++plan_revision;
}

void Ambient::PublishFrame(const QImage& frame, std::uint64_t sequence, bool enabled, const std::string& channel)
{
#ifdef AMBIENT_HAS_FRAME_SURFACE
    std::lock_guard<std::mutex> guard(publication_lock);
    if(!publication_running) return;
    if(!enabled || channel != publisher_channel)
    {
        publisher.reset();
        publisher_channel = channel;
        published_sequence = 0;
    }
    if(!enabled || frame.isNull()) return;
    if(!publisher)
    {
        publisher.reset(new room_surface::Publisher(channel));
        if(!publisher->IsOpen()) qWarning() << "[Ambient] Frame output:" << QString::fromStdString(publisher->LastError());
    }
    // A failed open is retried after toggling output or changing the channel.
    if(!publisher->IsOpen()) return;
    const bool changed = sequence != published_sequence || int(Brightness) != published_brightness
                         || Temperature != published_temperature || Tint != published_tint;
    const auto now = std::chrono::steady_clock::now();
    if(!changed && now - publication_time < std::chrono::milliseconds(500)) return;
    image_router.frame.Update(frame, sequence, Brightness, Temperature, Tint);
    const auto& output = image_router.frame;
    if(output.pixels && publisher->PublishBGRA(output.pixels->data(), output.pixels->size(), output.width, output.height, output.stride))
    {
        published_sequence = sequence;
        published_brightness = int(Brightness);
        published_temperature = Temperature;
        published_tint = Tint;
        publication_time = now;
    }
#else
    Q_UNUSED(frame); Q_UNUSED(sequence); Q_UNUSED(enabled); Q_UNUSED(channel);
#endif
}

void Ambient::StepEffect(std::vector<ControllerZone*> controller_zones)
{
    QImage frame;
    AmbientMode current_mode;
    unsigned int smoothing;
    bool output;
    std::string channel;
    std::shared_ptr<const json> regions;
    std::uint64_t sequence, revision;
    {
        std::lock_guard<std::mutex> guard(lock);
        frame = image; // Implicitly shared, immutable; no full-image deep copy per effect step.
        sequence = image_sequence;
        current_mode = mode;
        smoothing = smoothness;
        output = publish_frame;
        channel = frame_channel;
        regions = zone_regions;
        revision = plan_revision;
    }
    PublishFrame(frame, sequence, output, channel); // Also works with zero assigned LED zones.
    if(frame.isNull() || controller_zones.empty()) return;
    std::vector<ControllerZone*> led_zones;
    for(ControllerZone* zone : controller_zones)
        if(!image_router.Route(zone,frame,sequence,Brightness,Temperature,Tint,effect_canvas::RegionFor(zone,*regions)))
            led_zones.push_back(zone);
    if(current_mode == SCALED_AVERAGE)
    {
        if(averaged_sequence != sequence)
        {
            const QImage average = frame.scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            average_color = ColorUtils::fromQColor(average.pixelColor(0, 0));
            averaged_sequence = sequence;
        }
        for(ControllerZone* zone : led_zones)
            if(zone->leds_count()) zone->SetAllZoneLEDs(Smooth(zone->GetLED(0), average_color, smoothing), Brightness, Temperature, Tint);
        return;
    }
    for(ControllerZone* zone : led_zones)
    {
        const auto& samples = led_plans.Get(zone, revision, effect_canvas::RegionFor(zone, *regions));
        for(const auto& sample : samples)
        {
            const QRgb pixel = effect_canvas::SamplePixel(frame, sample);
            const RGBColor color = ToRGBColor(qRed(pixel), qGreen(pixel), qBlue(pixel));
            zone->SetLED(sample.led, Smooth(zone->GetLED(sample.led), color, smoothing), Brightness, Temperature, Tint);
        }
    }
}

RGBColor Ambient::Smooth(const RGBColor& previous_color, RGBColor color, unsigned int smoothing)
{
    return color == previous_color ? color : ColorUtils::Interpolate(previous_color, color, 0.01 * (100 - smoothing));
}

void Ambient::LoadCustomSettings(json settings)
{
    if(!settings.is_object()) return;
    auto set_number = [&](const char* key, auto setter)
    {
        if(settings.contains(key) && settings[key].is_number_integer())
        {
            const auto value = settings[key].get<std::int64_t>();
            if(value >= -100000 && value <= 100000) setter(int(value));
        }
    };
    set_number("left", [&](int v){ui->left->setValue(v);});
    set_number("top", [&](int v){ui->top->setValue(v);});
    set_number("width", [&](int v){ui->width->setValue(v);});
    set_number("height", [&](int v){ui->height->setValue(v);});
    set_number("mode", [&](int v){if(v >= 0 && v <= 1) ui->mode->setCurrentIndex(v);});
    set_number("screen_index", [&](int v){if(v >= 0 && v < ui->screen->count()) ui->screen->setCurrentIndex(v);});
    set_number("smoothness", [&](int v){ui->smoothness->setValue(v);});
    set_number("framerate", [&](int v){ui->framerate->setValue(v);});
    set_number("working_width", [&](int v){if(effect_canvas::ValidSize(v, working_height)) ui->working_width->setValue(v);});
    set_number("working_height", [&](int v){if(effect_canvas::ValidSize(working_width, v)) ui->working_height->setValue(v);});
    if(settings.contains("crop_stream") && settings["crop_stream"].is_boolean()) ui->crop_stream->setChecked(settings["crop_stream"]);
    if(settings.contains("frame_channel") && settings["frame_channel"].is_string())
        ui->frame_channel->setText(QString::fromStdString(settings["frame_channel"]));
    if(settings.contains("publish_frame") && settings["publish_frame"].is_boolean()) ui->publish_frame->setChecked(settings["publish_frame"]);
    const json valid_regions = effect_canvas::ValidRegions(settings.value("zone_regions", json::array()));
    std::lock_guard<std::mutex> guard(lock);
    zone_regions = std::make_shared<const json>(valid_regions);
    ++plan_revision;
    if(settings.contains("restore_token") && settings["restore_token"].is_string())
    {
        restore_token = QString::fromStdString(settings["restore_token"]);
        capturer->SetToken(restore_token);
    }
}

json Ambient::SaveCustomSettings()
{
    std::lock_guard<std::mutex> guard(lock);
    return {{"left", left}, {"top", top}, {"width", width}, {"height", height}, {"mode", mode},
            {"screen_index", screen_index}, {"smoothness", smoothness}, {"framerate", framerate},
            {"restore_token", restore_token.toStdString()}, {"crop_stream", crop_stream},
            {"working_width", working_width}, {"working_height", working_height},
            {"publish_frame", publish_frame}, {"frame_channel", frame_channel}, {"zone_regions", *zone_regions}};
}

void Ambient::on_left_valueChanged(int value)
{
    std::lock_guard<std::mutex> guard(lock); left = value; ++config_revision;
}
void Ambient::on_top_valueChanged(int value)
{
    std::lock_guard<std::mutex> guard(lock); top = value; ++config_revision;
}
void Ambient::on_width_valueChanged(int value)
{
    std::lock_guard<std::mutex> guard(lock); width = value; ++config_revision;
}
void Ambient::on_height_valueChanged(int value)
{
    std::lock_guard<std::mutex> guard(lock); height = value; ++config_revision;
}
void Ambient::on_mode_currentIndexChanged(int value)
{
    std::lock_guard<std::mutex> guard(lock); mode = value == SCREEN_COPY ? SCREEN_COPY : SCALED_AVERAGE;
}
void Ambient::on_screen_currentIndexChanged(int value)
{
    { std::lock_guard<std::mutex> guard(lock); screen_index = value; ++config_revision; image = {}; }
    if(capturer) capturer->SetScreen(value);
}
void Ambient::on_select_rectangle_clicked()
{
    rectangle_selector_overlay->StartSelection(screen_index);
}
void Ambient::on_select_screen_clicked()
{
    capturer->Init("", EffectEnabled);
}
void Ambient::on_smoothness_valueChanged(int value)
{
    std::lock_guard<std::mutex> guard(lock); smoothness = value;
}
void Ambient::on_framerate_valueChanged(int value)
{
    { std::lock_guard<std::mutex> guard(lock); framerate = value; }
    if(capturer) capturer->SetFrameRate(value);
}
void Ambient::on_crop_stream_stateChanged(int value)
{
    { std::lock_guard<std::mutex> guard(lock); crop_stream = value; ++config_revision; }
    ui->crop_frame->setVisible(value);
}
void Ambient::on_working_width_valueChanged(int value)
{
    std::lock_guard<std::mutex> guard(lock);
    if(effect_canvas::ValidSize(value, working_height)) { working_width = value; ++config_revision; }
}
void Ambient::on_working_height_valueChanged(int value)
{
    std::lock_guard<std::mutex> guard(lock);
    if(effect_canvas::ValidSize(working_width, value)) { working_height = value; ++config_revision; }
}
void Ambient::on_publish_frame_stateChanged(int value)
{
    std::lock_guard<std::mutex> guard(lock); publish_frame = value;
}
void Ambient::on_frame_channel_textChanged(const QString& value)
{
    const std::string channel = value.toStdString();
    const bool valid = !channel.empty() && channel.size() <= 64 && std::all_of(channel.begin(), channel.end(), [](unsigned char c)
    { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'; });
    if(valid) { std::lock_guard<std::mutex> guard(lock); frame_channel = channel; }
    ui->frame_channel->setToolTip(valid ? tr("Local FrameSurface channel") : tr("Use 1–64 letters, digits, hyphens or underscores; the last valid channel remains active."));
}
