// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "DynamicShaderImage.h"
#include "ShaderPass.h"
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

// An immutable acyclic GPU graph, independent of any capture provider. Sources
// are full GLSL mainImage bodies; each pass explicitly chooses four inputs.
struct ShaderRenderGraph
{
    struct Pass
    {
        std::string id,fragment;
        std::array<std::string,4> inputs;
        unsigned width=0,height=0;
        ShaderUniformMap uniforms;
    };
    std::vector<Pass> passes;
    std::string output;
};
struct ShaderRenderGraphFrame
{
    std::shared_ptr<const ShaderRenderGraph> graph;
    std::map<std::string,std::shared_ptr<const DynamicShaderImage>> images;
    std::chrono::steady_clock::time_point expires{};
};

// All methods, construction and destruction require a current GL context.
// The final image is read back once; intermediate images stay on the GPU.
class ShaderRenderGraphRunner
{
public:
    ShaderRenderGraphRunner();
    ~ShaderRenderGraphRunner();
    QImage Draw(const ShaderRenderGraphFrame&,unsigned outputWidth,unsigned outputHeight);
    ShaderRenderGraphRunner(const ShaderRenderGraphRunner&)=delete;
    ShaderRenderGraphRunner& operator=(const ShaderRenderGraphRunner&)=delete;
private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
