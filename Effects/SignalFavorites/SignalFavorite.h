// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "Shaders.h"
#include <chrono>
#include <map>
#include "TapHistory.h"
#include "KeyboardIdentity.h"
#include "KeyboardDeduplication.h"
#include "BasicEffectState.h"
#include "PumpDynamics.h"
#include "ProceduralEffectState.h"
#include "ScreenEffectState.h"
#include "ScreenSourceSelection.h"
#include "DominantScreenColor.h"
#include <FrameRouting/OpenRGBInputPluginAPI.h>

// Data-driven native effects: parameters remain uniforms, never shader source
// assembled from user values. The existing canvas router handles every device.
class SignalFavorite : public Shaders
{
public:
    explicit SignalFavorite(const QString& resource, QWidget* parent = nullptr);
    ~SignalFavorite() override;
    void StepEffect(std::vector<ControllerZone*>) override;
    void EffectState(bool) override;
    void LoadCustomSettings(json) override;
    json SaveCustomSettings() override;
    void OnControllerZonesListChanged(std::vector<ControllerZone*>) override;
    void SetSpeed(unsigned int) override { Shaders::SetSpeed(1000); }
    static void RegisterPresets();

protected:
    bool eventFilter(QObject*, QEvent*) override;
    void changeEvent(QEvent* event) override { QWidget::changeEvent(event); }

private:
    json spec;
    json parameters = json::object();
    std::mutex parameters_mutex;
    std::map<std::string,double> phases;
    std::map<std::string,QWidget*> editors;
    std::chrono::steady_clock::time_point previous_tick;
    bool clock_running = false;
    std::array<float,4> tap{0,0,0,0};
    native_taps::History tap_history;
    native_taps::KeyboardIdentity input_identity;
    native_taps::KeyboardDeduplication input_deduplication;
    native_basic::State basic_state;
    native_pump::State pump_state;
    native_procedural::State procedural_state;
    std::unique_ptr<native_screen::State> screen_state;
    native_screen_color::State screen_color;
    bool screen_capture_running=false;
    ScreenSourceSelection* screen_source = nullptr;
    std::shared_ptr<const DynamicShaderImage> last_screen_frame;
    std::uint64_t screen_frame_revision=0, screen_generation=1;
    unsigned pending_helper_taps = 0;
    room_input::PluginAPI* input_api = nullptr;
    std::uint64_t input_listener = 0;
    bool keyboard_enabled = true;
    bool effect_enabled = false;
    QCheckBox* keyboard_checkbox = nullptr;
    void UpdateInputListener();
    void CollectKeyboardTaps(const std::vector<ControllerZone*>&, double speed, double now);
    QSpinBox* canvas_width = nullptr;
    QSpinBox* canvas_height = nullptr;
    QLabel* preview = nullptr;
    void SetParameter(const std::string&, const json&);
    json Normalize(const json& control, const json& value) const;
    void SyncEditors();
    void SyncScreenCapture();
    void InstallProgram(const QString& resource);
};
