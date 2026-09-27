// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "CoreTypes.h"
#include <memory>

namespace room_ai
{
struct VideoStats
{
    std::size_t event_count=0;
    double confidence=0;
    bool cut=false;
    double frame_ms=0;
    std::uint64_t rejected_frames=0;
    std::string mode="Waiting for video";
    Vec2 camera_velocity{}; // robust coarse translation, screen-width units / s
    double camera_confidence=0; // heuristic spatial support, not a probability
};

struct VideoTexture
{
    unsigned width=1,height=1;
    std::shared_ptr<const std::vector<float>> rgba; // RGBA32F, texel (0,0) top-left
};
struct VideoRenderState
{
    VideoTexture grid,events;
    std::uint64_t revision=1;
    std::size_t event_count=0;
    double source_time=0,alive_time=0,screen_height=.5625,persistence=.8,strength=.7;
    bool valid=false,suppress_prediction=false;
};

// Single-owner computational engine: callers serialize Push/settings/Sample.
// Coordinates are screen widths, with the observed rectangle [0,1] x [0,height].
// Owns a small linear-light grid, never retains caller-owned pixel storage.
class VideoEngine
{
public:
    VideoEngine();
    ~VideoEngine();
    VideoEngine(VideoEngine&&) noexcept;
    VideoEngine& operator=(VideoEngine&&) noexcept;
    VideoEngine(const VideoEngine&)=delete;
    VideoEngine& operator=(const VideoEngine&)=delete;
    void Reset();
    bool Push(const VideoFrame& frame);
    // Only call with the consumer's current clock after Source::Usable succeeds.
    // Liveness does not observe motion or prolong event TTL; pixel planes/revision
    // stay shared. A caller cannot fabricate future producer timestamps here.
    bool Refresh(double now);
    Color Sample(Vec2 point,double now,bool predictive) const;
    std::vector<Color> Sample(const std::vector<Vec2>& points,double now,bool predictive) const;
    VideoStats Stats() const;
    // Cached immutable numeric export; old snapshots survive Push/Reset/settings.
    // DynamicShaderImage can share rgba without a CPU pixel copy. Only iaAge
    // changes between frames; no upload is required for an identical revision.
    std::shared_ptr<const VideoRenderState> RenderState() const;
    void SetPersistence(double seconds); // finite values clamped to .1..5 s
    void SetStrength(double strength);   // finite values clamped to 0..1
    void SetScreenHeight(double height); // finite .1..4; geometry change resets memory
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
