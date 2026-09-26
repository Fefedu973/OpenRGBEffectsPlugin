/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace WindowsCapture
{
enum class Rotation { Identity, Clockwise90, Clockwise180, Clockwise270 };

inline bool ValidSize(unsigned int width, unsigned int height)
{
    return width && height && width <= 16384 && height <= 16384
        && static_cast<std::uint64_t>(width) * height <= 64ULL * 1024 * 1024;
}

inline bool SwapsAxes(Rotation rotation)
{
    return rotation == Rotation::Clockwise90 || rotation == Rotation::Clockwise270;
}

/* DXGI supplies an unrotated BGRA surface. Copy into the display orientation,
 * respecting both pitches. Alpha is made opaque for QImage::Format_RGB32.
 * This is the explicit GPU readback -> owned CPU pixels boundary. */
inline bool CopyBgra(const void* source, std::size_t source_pitch,
                     unsigned int width, unsigned int height, Rotation rotation,
                     void* destination, std::size_t destination_pitch)
{
    const unsigned int output_width = SwapsAxes(rotation) ? height : width;
    if(!source || !destination || !ValidSize(width, height)
       || source_pitch < static_cast<std::size_t>(width) * 4
       || destination_pitch < static_cast<std::size_t>(output_width) * 4)
    {
        return false;
    }
    const auto* input = static_cast<const unsigned char*>(source);
    auto* output = static_cast<unsigned char*>(destination);
    for(unsigned int y = 0; y < height; ++y)
    {
        const auto* row = input + source_pitch * y;
        if(rotation == Rotation::Identity)
        {
            auto* target = output + destination_pitch * y;
            std::memcpy(target, row, static_cast<std::size_t>(width) * 4);
            for(unsigned int x = 0; x < width; ++x) target[4 * x + 3] = 255;
            continue;
        }
        for(unsigned int x = 0; x < width; ++x)
        {
            unsigned int target_x = x;
            unsigned int target_y = y;
            switch(rotation)
            {
            case Rotation::Clockwise90:  target_x = height - 1 - y; target_y = x; break;
            case Rotation::Clockwise180: target_x = width - 1 - x; target_y = height - 1 - y; break;
            case Rotation::Clockwise270: target_x = y; target_y = width - 1 - x; break;
            default: break;
            }
            auto* target = output + destination_pitch * target_y + 4 * target_x;
            std::memcpy(target, row + 4 * x, 3);
            target[3] = 255;
        }
    }
    return true;
}

/* Acquired DXGI frames must be released on every exit, including failures
 * creating staging textures or mapping them. The fake tests exercise this
 * same guard with a duplication object that only counts ReleaseFrame calls. */
template<class Duplication>
class FrameLease
{
public:
    explicit FrameLease(Duplication* value) : duplication(value) {}
    ~FrameLease() { if(duplication) duplication->ReleaseFrame(); }
    FrameLease(const FrameLease&) = delete;
    FrameLease& operator=(const FrameLease&) = delete;
    auto Release() -> decltype(static_cast<Duplication*>(nullptr)->ReleaseFrame())
    {
        auto* value = duplication;
        duplication = nullptr;
        return value->ReleaseFrame();
    }
private:
    Duplication* duplication;
};

/* The clock is provided by the caller, making failover and recovery testable
 * without D3D, a display, a GUI application, or real waiting. */
class RetryPolicy
{
public:
    using Clock = std::chrono::steady_clock;
    using Time = Clock::time_point;
    bool ShouldTryDxgi(Time now) const { return !dxgi_active && now >= retry_at; }
    bool UsesDxgi() const { return dxgi_active; }
    void Opened() { dxgi_active = true; }
    void Failed(Time now) { dxgi_active = false; retry_at = now + std::chrono::seconds(5); }
    void Reset() { dxgi_active = false; retry_at = Time::min(); }
private:
    bool dxgi_active = false;
    Time retry_at = Time::min();
};

inline std::chrono::microseconds FramePeriod(unsigned int fps, bool fallback)
{
    /* GDI is an emergency fallback, capped at 15 fps. DXGI tracks configured
     * capture FPS (1..240). Acquire/Map time is included in this period. */
    fps = std::max(1U, std::min(fps, fallback ? 15U : 240U));
    return std::chrono::microseconds(1000000 / fps);
}
}
