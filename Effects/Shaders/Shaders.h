/*---------------------------------------------------------*\
| Shaders.h                                                 |
|                                                           |
|   OpenRGB Effects Plugin Shaders Effect                   |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include "AudioSignalProcessor.h"
#include "AudioSettings.h"
#include "AudioSettingsStruct.h"

#include <QWidget>
#include <QListWidget>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <mutex>
#include <memory>
#include <cstdint>
#if defined(_WIN32) && __has_include("FrameSurface/FrameSurface.h")
#include "FrameSurface/FrameSurface.h"
#define SHADERS_HAS_FRAME_SURFACE 1
#endif
#include <QFile>
#include <QMessageBox>
#include <QDirIterator>
#include "ui_Shaders.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "ShaderRenderer.h"
#include "GLSLCodeEditor.h"
#include "ShaderProgram.h"
#include "CanvasRouting.h"

namespace Ui {
class Shaders;
}

class Shaders : public RGBEffect
{
    Q_OBJECT

public:
    explicit Shaders(QWidget *parent = nullptr);
    ~Shaders();

    EFFECT_REGISTERER(ClassName(), UI_Name(), CAT_SPECIAL, [](){return new Shaders;});

    static std::string const ClassName() {return "Shaders";}
    static std::string const UI_Name() { return QT_TR_NOOP("Shaders"); }

    void StepEffect(std::vector<ControllerZone*>) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;

    void EffectState(bool) override;
    void SetFPS(unsigned int) override;
    void OnControllerZonesListChanged(std::vector<ControllerZone*>) override;


private slots:
    void changeEvent(QEvent *event) override;
    void on_show_rendering_stateChanged(int);
    void on_use_audio_stateChanged(int);
    void on_shaders_currentIndexChanged(int);
    void on_width_valueChanged(int);
    void on_height_valueChanged(int);
    void on_invert_time_stateChanged(int);
    void on_edit_clicked();
    void on_time_reset_clicked();
    void on_save_shader_as_clicked();
    void on_open_shaders_folder_clicked();

    void on_audio_settings_clicked();
    void OnAudioDeviceChanged(int);

private:
    Ui::Shaders *ui;

    void SetDynamicStrings();

    QImage image;
    ShaderRenderer* shader_renderer = nullptr;
    GLSLCodeEditor* editor = nullptr;
    GLSLHighlighter* highlighter = nullptr;
    float time = 0.f;
    unsigned int width = 128;
    unsigned int height = 128;
    std::vector<QString> shader_paths;
    unsigned int current_shader_idx = 0;
    bool show_rendering = false;
    std::mutex image_mutex;  
    uint64_t image_sequence = 0;
    uint64_t preview_sequence = 0;
    uint64_t published_sequence = 0;
    float published_brightness = -1;
    int published_temperature = 0, published_tint = 0;
    bool publish_frame = false;
    std::string frame_channel = "room-shaders";
    std::shared_ptr<const json> zone_regions = std::make_shared<const json>(json::array());
    std::uint64_t plan_revision = 0;
    effect_canvas::LedPlans led_plans;
    effect_canvas::Router image_router;
#ifdef SHADERS_HAS_FRAME_SURFACE
    std::mutex publication_mutex;
    bool publication_running = false;
    std::unique_ptr<room_surface::Publisher> frame_publisher;
    std::string publisher_channel;
#endif
    bool use_audio = false;
    bool invert_time = false;

    AudioSettings                   audio_settings;
    Audio::AudioSettingsStruct      audio_settings_struct;
    AudioSignalProcessor            audio_signal_processor;

    void Resize();
    void StartAudio();
    void StopAudio();
    void HandleAudioCapture();
};
