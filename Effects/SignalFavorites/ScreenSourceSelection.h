// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QGroupBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include "ScreenCapturer.h"
#include "DynamicShaderImage.h"
#include "ScreenSources/ScreenSource.h"
#include "ScreenSources/BetterDiscovery.h"
#include "ScreenSources/BetterAppearanceInput.h"

// The UI owns capture lifecycle. The effect worker only reads an immutable
// mailbox; it never performs capture, network access or shared-memory I/O.
class ScreenSourceSelection : public QGroupBox
{
public:
    explicit ScreenSourceSelection(QWidget* parent=nullptr,bool offerAppearance=false);
    ~ScreenSourceSelection() override;
    void SetRunning(bool);
    void Load(const nlohmann::json&);
    nlohmann::json Save() const;
    std::shared_ptr<const DynamicShaderImage> Latest() const;
    std::shared_ptr<const ShaderRenderGraphFrame> LatestAppearance() const;
    void SetOutputSize(unsigned width,unsigned height);
private:
    void Reconfigure();
    void RefreshStatus();
    void RefreshBetter();
    QComboBox *kind=nullptr,*display=nullptr,*scene=nullptr;
    QCheckBox* follow_appearance=nullptr;
    QLineEdit* channel=nullptr;
    QLineEdit* descriptor=nullptr;
    QLabel* status=nullptr;
    std::unique_ptr<ScreenCapturer> capturer;
    mutable std::mutex mutex;
    std::shared_ptr<screen_source::Source> external;
    std::shared_ptr<better_source::Discovery> discovery;
    std::shared_ptr<better_source::FrameSource> paired;
    std::shared_ptr<better_source::AppearanceInput> appearance;
    QString owner,requested_scene,claimed_scene;
    bool appearance_active=false;
    bool scene_required=false,scene_invalid=false;
    unsigned output_width=800,output_height=500;
    QString bound_channel,bound_instance;
    std::shared_ptr<const DynamicShaderImage> native;
    std::uint64_t generation=0,sequence=0;
    bool running=false, native_active=false;
    bool destroying=false;
    QString native_status;
    QString source_error;
};
