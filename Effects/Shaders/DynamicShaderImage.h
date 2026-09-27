// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QImage>
#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

// Immutable, latest-frame input. Top-left is texel (0,0), without an implicit
// vertical flip. A numeric texture uses nearest sampling; color uses linear.
// GL resources belong only to ShaderPass, never to a producer/capture worker.
struct DynamicShaderImage
{
    QImage image;
    std::shared_ptr<const std::vector<float>> rgba32f;
    unsigned width = 0, height = 0;
    std::uint64_t sequence = 0, generation = 0;
    std::uint64_t source_revision = 0; // Local source selection, not a wire ABI field.
    bool metadata_generation = false; // Better can rotate state without restarting capture.
    std::chrono::steady_clock::time_point expires = std::chrono::steady_clock::time_point::max();

    unsigned Width() const { return rgba32f ? width : unsigned(image.width()); }
    unsigned Height() const { return rgba32f ? height : unsigned(image.height()); }
    bool Valid() const
    {
        const auto w = Width(), h = Height();
        if(!w || !h || w > 4096 || h > 4096 || !sequence) return false;
        const auto bytes = std::uint64_t(w) * h * (rgba32f ? 16 : 4);
        if(bytes > 64ULL * 1024 * 1024) return false;
        return rgba32f ? rgba32f->size() == std::uint64_t(w) * h * 4 : !image.isNull();
    }
    bool Usable() const { return Valid() && std::chrono::steady_clock::now() <= expires; }
};
