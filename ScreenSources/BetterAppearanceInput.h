// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "BetterFrameSource.h"
#include "Effects/Shaders/ShaderRenderGraph.h"

namespace better_source
{
// Preparation follows the immutable rendering recipe, off the GUI/render
// threads. No HTTP, capture, settings mutation or JavaScript execution here.
class AppearanceInput final
{
public:
    AppearanceInput(std::shared_ptr<FrameSource>,unsigned width,unsigned height);
    ~AppearanceInput();
    std::shared_ptr<const ShaderRenderGraphFrame> Read() const;
    QString Status() const;
    void Resize(unsigned width,unsigned height);
private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
}
