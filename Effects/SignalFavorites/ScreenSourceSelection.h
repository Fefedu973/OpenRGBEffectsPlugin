// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QGroupBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include "ScreenCapturer.h"
#include "DynamicShaderImage.h"
#include "ScreenSources/ScreenSource.h"

// The UI owns capture lifecycle. The effect worker only reads an immutable
// mailbox; it never performs capture, network access or shared-memory I/O.
class ScreenSourceSelection : public QGroupBox
{
public:
    explicit ScreenSourceSelection(QWidget* parent=nullptr);
    ~ScreenSourceSelection() override;
    void SetRunning(bool);
    void Load(const nlohmann::json&);
    nlohmann::json Save() const;
    std::shared_ptr<const DynamicShaderImage> Latest() const;
private:
    void Reconfigure();
    void RefreshStatus();
    QComboBox *kind=nullptr,*display=nullptr;
    QLineEdit* channel=nullptr;
    QLabel* status=nullptr;
    std::unique_ptr<ScreenCapturer> capturer;
    mutable std::mutex mutex;
    std::shared_ptr<screen_source::Source> external;
    std::shared_ptr<const DynamicShaderImage> native;
    std::uint64_t generation=0,sequence=0;
    bool running=false, native_active=false;
    QString native_status;
    QString source_error;
};
