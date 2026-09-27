// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "Shaders.h"
#include <chrono>
#include <map>

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
    QSpinBox* canvas_width = nullptr;
    QSpinBox* canvas_height = nullptr;
    QLabel* preview = nullptr;
    void SetParameter(const std::string&, const json&);
    json Normalize(const json& control, const json& value) const;
    void SyncEditors();
    void InstallProgram(const QString& resource);
};
