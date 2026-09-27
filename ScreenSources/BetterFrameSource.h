/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "BetterDiscovery.h"
#include <QImage>
#include <QJsonObject>
#include <chrono>
#include <cstdint>
#include <memory>

namespace better_source
{
enum class FrameState { Starting, Ready, WaitingDiscovery, WaitingRaw, WaitingState,
                        WaitingCoverage, Invalid, Stale, Stopped };
const char* FrameStateName(FrameState state);

struct FrameSnapshot
{
    FrameState state = FrameState::Starting;
    QString detail, instance_id;
    std::uint64_t revision = 0;
    QImage raw, coverage; // Immutable owned BGRA/ARGB32, opaque, top-left origin.
    QJsonObject metadata; // Exactly the bound envelope's "rendering" object.
    std::uint64_t raw_generation = 0, raw_sequence = 0;
    std::uint64_t coverage_generation = 0, coverage_sequence = 0;
    std::uint64_t state_revision = 0, control_revision = 0, scene_revision = 0;
    QString scene_id;
    std::chrono::steady_clock::time_point expires{};
    bool Ready() const { return state == FrameState::Ready; }
    bool Usable() const;
};

// Passive consumer: never acquires a scene lease or changes Better appearance.
// One shared worker per Discovery; Read does no HTTP, mapping, or pixel work.
// Discovery owns HTTP/authentication; ScreenSource owns bounded surface reads.
class FrameSource final
{
public:
    static std::shared_ptr<FrameSource> Acquire(std::shared_ptr<Discovery> discovery);
    ~FrameSource();
    FrameSource(const FrameSource&) = delete;
    FrameSource& operator=(const FrameSource&) = delete;
    std::shared_ptr<const FrameSnapshot> Read() const;
private:
    class Impl;
    explicit FrameSource(std::shared_ptr<Discovery> discovery);
    std::unique_ptr<Impl> impl;
};
}
