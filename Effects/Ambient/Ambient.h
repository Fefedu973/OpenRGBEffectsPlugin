/*---------------------------------------------------------*\
| Ambient.h                                                 |
|                                                           |
|   OpenRGB Effects Plugin Ambient Effect                   |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <mutex>
#include <memory>
#include <map>
#include <chrono>
#include <QWidget>
#include <QMouseEvent>
#include "ui_Ambient.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "RectangleSelector.h"
#include "ScreenCapturer.h"
#include "CanvasImage.h"
#include "CanvasRegions.h"
#include "CanvasRouting.h"

#if defined(_WIN32) && __has_include(<FrameSurface/FrameSurface.h>)
#include <FrameSurface/FrameSurface.h>
#define AMBIENT_HAS_FRAME_SURFACE 1
#endif

namespace Ui {
class Ambient;
}

enum AmbientMode{
    SCALED_AVERAGE = 0,
    SCREEN_COPY = 1
};

class Ambient : public RGBEffect
{
    Q_OBJECT

public:
    explicit Ambient(QWidget *parent = nullptr);
    ~Ambient();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SPECIAL, [](){return new Ambient;});

    static std::string const ClassName() {return "Ambient";}
    static std::string const UI_Name() { return QT_TR_NOOP("Ambient"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;
    void EffectState(bool) override;
    void OnControllerZonesListChanged(std::vector<ControllerZone*>) override;

private slots:
    void changeEvent(QEvent *event) override;
    void on_select_screen_clicked();
    void on_select_rectangle_clicked();
    void on_left_valueChanged(int);
    void on_top_valueChanged(int);
    void on_width_valueChanged(int);
    void on_height_valueChanged(int);
    void on_mode_currentIndexChanged(int);
    void on_screen_currentIndexChanged(int);
    void on_smoothness_valueChanged(int);
    void on_framerate_valueChanged(int);
    void on_crop_stream_stateChanged(int);
    void on_working_width_valueChanged(int);
    void on_working_height_valueChanged(int);
    void on_publish_frame_stateChanged(int);
    void on_frame_channel_textChanged(const QString&);

private:
    Ui::Ambient *ui;
    ScreenCapturer* capturer = nullptr;
    RectangleSelectorOverlay* rectangle_selector_overlay = nullptr;

    int screen_index = -1;
    AmbientMode mode = SCALED_AVERAGE;

    void UpdateSelection();
    void SetDynamicStrings();
    RGBColor Smooth(const RGBColor& previous_color, RGBColor color, unsigned int smoothing);
    void ReceiveImage(const QImage&);
    void PublishFrame(const QImage&, std::uint64_t sequence, bool enabled, const std::string& channel);

    unsigned int left = 0;
    unsigned int top = 0;
    unsigned int width = 1;
    unsigned int height = 1;
    unsigned int smoothness = 80;
    unsigned int framerate = 60;
    bool crop_stream = false;
    int working_width = 800, working_height = 600;
    bool publish_frame = false;
    std::string frame_channel = "room-ambient";
    std::shared_ptr<const json> zone_regions = std::make_shared<const json>(json::array());
    std::uint64_t config_revision = 0, image_sequence = 0, plan_revision = 0;
    QString restore_token;

    QImage image;
    std::mutex lock;

    // Effect worker owns these; the capture callback only replaces the image mailbox.
    effect_canvas::LedPlans led_plans;
    std::uint64_t averaged_sequence = 0;
    RGBColor average_color = 0;
    effect_canvas::Router image_router;
#ifdef AMBIENT_HAS_FRAME_SURFACE
    std::mutex publication_lock;
    bool publication_running = false;
    std::unique_ptr<room_surface::Publisher> publisher;
    std::string publisher_channel;
    std::uint64_t published_sequence = 0;
    int published_brightness = -1, published_temperature = 0, published_tint = 0;
    std::chrono::steady_clock::time_point publication_time{};
#endif
};
