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
#include <QFile>
#include <QMessageBox>
#include <QDirIterator>
#include "ui_Shaders.h"
#include "RGBEffect.h"
#include "EffectRegisterer.h"
#include "ShaderRenderer.h"
#include "GLSLCodeEditor.h"
#include "ShaderProgram.h"

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
