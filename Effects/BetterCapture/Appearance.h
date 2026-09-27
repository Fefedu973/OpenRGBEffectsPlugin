// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QImage>
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace better_capture {
struct Uniform { std::array<float,4> value{}; unsigned components=1; };
using Uniforms=std::map<std::string,Uniform>;
struct Pass {
    std::string id,shader;
    std::array<std::string,4> inputs{};
    unsigned width=0,height=0;
    Uniforms uniforms;
};
struct Graph {
    std::string preamble="shaders/BetterCapture/common.glsl";
    std::vector<Pass> passes;
    std::string output;
};
struct Prepared {
    Graph graph;
    // Geometry only: no capture pixels. Numeric atlas uses nearest sampling.
    // First half: quantized seed UV, antialiased silhouette, seed validity.
    // Second half: three conservative sampling limits, occupancy.
    QImage silhouette;
    std::shared_ptr<const std::vector<float>> geometryRGBA;
    unsigned geometryWidth=0,geometryHeight=0;
};

// Pure preparation: caller already validated the raw-generation/coverage pair.
// Cache by immutable rendering state + output dimensions; do not call per frame.
// Throws invalid_argument for malformed schema/geometry or excessive dimensions.
// External graph resources: raw, coverage, geometry. All shaders are GLSL130.
Prepared Prepare(const nlohmann::json& rendering,unsigned outputWidth,unsigned outputHeight);
}
