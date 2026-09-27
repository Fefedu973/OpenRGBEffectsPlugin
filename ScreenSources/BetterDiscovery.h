/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <cstdint>
#include <memory>

// Native Better discovery, independent of image transport and control leases.
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
private:
    class Impl;
    explicit Discovery(const Config& config);
    std::unique_ptr<Impl> impl;
};
}
