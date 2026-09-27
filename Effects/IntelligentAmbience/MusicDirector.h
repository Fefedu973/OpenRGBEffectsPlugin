// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "CoreTypes.h"
#include "Audio/RhythmTracker.h"
#include <memory>

namespace room_ai
{
// Plain values suitable for shader uniforms. RGB is linear; world units use
// screen width=1. No audio clock is converted to a float shader timestamp.
struct MusicRenderState
{
    bool valid=false;
    Color palette{};
    // Base light level, intensity, extrapolated beat phase, tempo enabled.
    std::array<float,4> controls{};
    float motion_phase=0;
    std::array<float,8> bands{};
    // x, y, current (already decayed) strength, unused. Exactly 16 slots.
    std::array<std::array<float,4>,16> accents{};
};

// Consumes the existing capture's observations, never re-analyzes PCM or opens
// capture. Push once per render update, including silent/unavailable snapshots.
// now and input.audio_time MUST share one monotonic clock (QPC seconds on MSVC).
// Read capture first, then read now; do not pass an effect-relative iTime.
// Writers are serialized; readers copy immutable published values.
class MusicDirector
{
public:
    MusicDirector();
    ~MusicDirector();
    MusicDirector(const MusicDirector&)=delete;
    MusicDirector& operator=(const MusicDirector&)=delete;
    void Reset();
    void Push(const room_audio::RhythmSnapshot& input,double now);
    MusicSnapshot Snapshot(double now) const;
    MusicRenderState RenderState(double now) const;
    Color Sample(Vec2 point,double now,bool directed) const;
    void SetIntensity(double value);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
