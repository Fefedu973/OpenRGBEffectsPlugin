// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "Shaders.h"
#include "ScreenSourceSelection.h"
#include "VideoEngine.h"
#include "MusicDirector.h"
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
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
    QComboBox* mode=nullptr;QCheckBox *demo=nullptr,*predictive=nullptr;
    QDoubleSpinBox *persistence=nullptr,*strength=nullptr,*hybrid=nullptr;
    std::array<QDoubleSpinBox*,4> screen_fields{};
    QLabel* status=nullptr;QString status_text;
    void ControlsChanged();void SyncControls();void ConfigureCapture();void InstallProgram();
    static Parameters Parse(const json&);
    static json Serialize(const Parameters&);
};
