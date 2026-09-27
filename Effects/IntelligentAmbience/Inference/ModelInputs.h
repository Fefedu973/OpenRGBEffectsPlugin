// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "InferenceWorker.h"
#include "../../Shaders/DynamicShaderImage.h"
#include "../../../Audio/PcmWindow.h"
#include <QImage>
#include <optional>

namespace room_ai::inference
{
// Worker-owned adapters. They never capture, access a device, load a model or
// touch GL. Call Reset on a new model/source. Rejected/duplicate observations
// return no request; valid discontinuities use current==previous and delta=0.
class VideoAdapter
{
public:
    std::optional<Request> Make(const ModelConfig&, const QImage& owned_image,
        std::uint64_t generation, std::uint64_t source_epoch,
        std::uint64_t sequence, double source_time,
        const std::array<double,4>& normalized_screen_rect);
    void Reset();
private:
    std::vector<float> previous;
    std::vector<std::int64_t> shape;
    std::array<double,4> rect{};
    std::uint64_t generation=0,epoch=0,sequence=0;
    int source_width=0,source_height=0;
    double time=0;
};

// Exact v1 PCM sample count, or zero for an invalid/inapplicable contract.
std::size_t AudioSamples(const ModelConfig&);
class AudioAdapter
{
public:
    std::optional<Request> Make(const ModelConfig&,
        const room_audio::PcmWindowSnapshot&, std::uint64_t generation, double now);
    void Reset();
private:
    std::uint64_t generation=0,epoch=0,sequence=0;
    std::size_t samples=0;
    double time=0;
};

// Model output is linear RGB NCHW -> immutable top-left RGBA32F. Confidence
// occupies alpha; RGB remains unpremultiplied. No color encoding occurs here.
// Invalid/nonfinite/mismatched/stale output is rejected as a whole.
std::shared_ptr<const DynamicShaderImage> FieldImage(
    const Result&, const ModelConfig&, double now);
}
