/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QJsonObject>
#include <cstdint>
#include <memory>

// Native Better discovery, generation metadata and optional scene leases.
// Image pixels remain on the separate shared-memory transport.
// Read()/Refresh() never perform I/O on the caller's thread. Credentials remain
// private to the worker and are neither returned, logged nor persisted.
namespace better_source
{
enum class State { Starting, Ready, MissingDescriptor, InvalidDescriptor, Unavailable, Unauthorized, Incompatible, Stopped };
const char* StateName(State state);
struct Config
{
    QString descriptor_path; // Empty selects DefaultDescriptorPath().
    unsigned refresh_ms=1000;
    unsigned request_timeout_ms=2000;
};
struct Scene { QString id,name; };
struct Snapshot
{
    State state=State::Starting;
    QString detail;
    std::uint64_t revision=0;
    QString instance_id;
    std::uint32_t process_id=0;
    QString raw_channel,coverage_channel;
    unsigned canvas_width=0,canvas_height=0,recommended_ttl_ms=2000;
    QStringList capabilities;
    QVector<Scene> scenes;
    bool Ready() const { return state==State::Ready; }
};
enum class PublishedStateStatus { Pending, Ready, Unavailable, Invalid };
struct PublishedState
{
    PublishedStateStatus status=PublishedStateStatus::Pending;
    QString instance_id,detail;
    std::uint64_t requested_generation=0;
    QJsonObject envelope;
};
enum class RequestResult { Accepted, Conflict, Invalid };
enum class ControlPhase { Idle, Waiting, Pending, Effective, Conflict, Superseded, Unavailable, Releasing };
struct ControlSnapshot
{
    ControlPhase phase=ControlPhase::Idle;
    QString detail,scene_id,instance_id;
    std::uint64_t revision=0,control_revision=0,scene_revision=0;
    std::uint64_t effective_generation=0,minimum_sequence=0;
};
class Discovery final
{
public:
    static QString DefaultDescriptorPath();
    // Identical normalized configs share a worker. Invalid explicit configs
    // throw invalid_argument; a missing descriptor is an ordinary snapshot.
    static std::shared_ptr<Discovery> Acquire(const Config& config=Config{});
    ~Discovery();
    Discovery(const Discovery&)=delete;
    Discovery& operator=(const Discovery&)=delete;
    std::shared_ptr<const Snapshot> Read() const;
    void Refresh();
    // Latest-generation request, bounded8-entry immutable cache. No callback.
    // A Ready response is authenticated JSON; validate full frame association
    // separately with BetterControlState::ValidateFrameBinding.
    void RequestState(std::uint64_t generation);
    std::shared_ptr<const PublishedState> ReadState(std::uint64_t generation) const;
    // One application-wide scene: equal scenes share one lease, conflicting
    // scenes are refused. Owner IDs are local effect-instance IDs (1..128 chars).
    // Empty scene UUID is equivalent to ReleaseScene(owner). No appearance edit.
    RequestResult RequestScene(const QString& owner,const QString& scene_uuid);
    void ReleaseScene(const QString& owner);
    std::shared_ptr<const ControlSnapshot> ReadControl(const QString& owner) const;
private:
    class Impl;
    explicit Discovery(const Config& config);
    std::unique_ptr<Impl> impl;
};
}
