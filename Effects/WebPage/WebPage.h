/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "CanvasRouting.h"
#include "WebPageCapture.h"
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <chrono>
#if defined(_WIN32) && __has_include(<FrameSurface/FrameSurface.h>)
#include <FrameSurface/FrameSurface.h>
#define WEBPAGE_FRAME_SURFACE 1
#endif

class WebPage : public RGBEffect
{
    Q_OBJECT
public:
    explicit WebPage(QWidget* parent=nullptr);
    ~WebPage() override;
    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SPECIAL, [](){return new WebPage;});
    static std::string const ClassName() { return "WebPage"; }
    static std::string const UI_Name() { return QT_TR_NOOP("Web Page"); }
    void StepEffect(std::vector<ControllerZone*>) override;
    void EffectState(bool) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;
    void OnControllerZonesListChanged(std::vector<ControllerZone*>) override;
private:
    void Apply();
    void RestartBrowser();
    WebPageCapture* capture;
    QLineEdit *url_edit,*channel_edit;
    QSpinBox *width_edit,*height_edit,*fps_edit;
    QCheckBox* publish_edit;
    QLabel* status;
    std::mutex mutex;
    bool running=false,publish=false;
    int width=800,height=600,fps=20;
    QString url;
    std::string channel="room-webpage";
    QImage image;
    std::uint64_t sequence=0,revision=0;
    std::chrono::steady_clock::time_point received{},last_publication{};
    json regions=json::array();
    effect_canvas::Router router;
    effect_canvas::LedPlans led_plans;
#ifdef WEBPAGE_FRAME_SURFACE
    std::unique_ptr<room_surface::Publisher> publisher;
    std::string publisher_channel;
    std::uint64_t published_sequence=0;
#endif
};
