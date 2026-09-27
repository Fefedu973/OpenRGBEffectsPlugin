// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "Shaders.h"
#include "ScreenSourceSelection.h"
#include "VideoEngine.h"
#include "MusicDirector.h"
#include "Inference/InferenceWorker.h"
#include "Inference/ModelInputs.h"
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
class IntelligentAmbience : public Shaders
{
public:
    explicit IntelligentAmbience(QWidget* parent=nullptr);
    ~IntelligentAmbience()override;
    static std::string const ClassName(){return "IntelligentAmbience";}
    static std::string const UI_Name(){return QT_TR_NOOP("Intelligent Ambience (prototype)");}
    EFFECT_REGISTERER(ClassName(),UI_Name(),CAT_ADVANCED,[]{return new IntelligentAmbience;});
    void StepEffect(std::vector<ControllerZone*>)override;
    void EffectState(bool)override;
    void SetSpeed(unsigned)override{Shaders::SetSpeed(1000);}
    void SetFPS(unsigned value)override{Shaders::SetFPS(std::clamp(value,1u,120u));}
    void LoadCustomSettings(json)override;
    json SaveCustomSettings()override;
protected:
    void changeEvent(QEvent* e)override{QWidget::changeEvent(e);}
private:
    struct Parameters
    {
        int mode=0;bool demo=false,predictive=true;
        double persistence=1,strength=.75,hybrid=.25;
        std::array<double,4> screen{80,55,160,90};
        std::array<bool,2> model_enabled{false,false};
        std::array<std::string,2> model_path;
        double inference_fps=10;
    } parameters;
    std::mutex state_mutex;
    room_ai::VideoEngine video;
    room_ai::MusicDirector music;
    ScreenSourceSelection* source=nullptr;
    std::shared_ptr<const DynamicShaderImage> previous_source;
    std::shared_ptr<const room_ai::VideoRenderState> previous_render;
    std::array<std::shared_ptr<const DynamicShaderImage>,4> inputs;
    std::uint64_t observation_sequence=0,stream_epoch=1,texture_generation=1;
    std::atomic<std::uint64_t> audio_epoch{1};
    double last_analysis=-1,origin=0,missing_since=-1;
    bool enabled=false,source_running=false;
    struct ModelSlot
    {
        room_ai::inference::Worker worker;
        std::string configured_key;
        std::uint64_t generation=0,sequence=0;
        double last_submit=-1;
        std::shared_ptr<room_ai::inference::VideoAdapter> video_adapter;
        std::shared_ptr<room_ai::inference::AudioAdapter> audio_adapter;
        std::shared_ptr<std::atomic<std::uint64_t>> audio_input_epoch;
        std::shared_ptr<const room_ai::inference::Result> result;
        std::string adaptation_error;
    } models[2];
    int current_audio_device=-1;
    QImage demo_image;
    bool previous_model_cut=false;
    std::array<QCheckBox*,2> model_checks{};
    std::array<QLineEdit*,2> model_paths{};
    std::array<QLabel*,2> model_status{};
    QDoubleSpinBox* inference_fps=nullptr;
    QComboBox* mode=nullptr;QCheckBox *demo=nullptr,*predictive=nullptr;
    QDoubleSpinBox *persistence=nullptr,*strength=nullptr,*hybrid=nullptr;
    std::array<QDoubleSpinBox*,4> screen_fields{};
    QLabel* status=nullptr;QString status_text;
    void ControlsChanged();void SyncControls();void ConfigureCapture();void InstallProgram();
    void ConfigureModels();
    void UpdateModels(const std::shared_ptr<const DynamicShaderImage>&,double,bool,ShaderUniformMap&);
    static Parameters Parse(const json&);
    static json Serialize(const Parameters&);
};
