/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <QImage>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

// Shared image INPUT for capture-consuming effects. This is independent of the
// CanvasRouting/VisualMap image OUTPUT API. Read() never performs producer I/O.
namespace screen_source
{
enum class State { Starting, Live, Static, Busy, Stale, Unavailable, Invalid, Unsupported, Stopped };
const char* StateName(State state);

struct Config
{
    std::string channel = "better-screen-capture";
    unsigned poll_ms = 16;
    unsigned ttl_ms = 2000;
    unsigned max_width = 4096;
    unsigned max_height = 4096;
    bool Valid() const;
};

struct Frame
{
    // Owned, implicitly shared pixels. No alias to the producer mapping remains.
    // Treat as immutable; modifying a QImage copy detaches it in the normal Qt way.
    QImage image;
    std::uint64_t sequence = 0, generation = 0, timestamp_ms = 0;
    std::chrono::steady_clock::time_point received{}, expires{};
};

struct Snapshot
{
    State state = State::Starting;
    std::shared_ptr<const Frame> frame;
    std::uint64_t revision = 0;
    std::string detail;
    // A contended read can retain the last frame only within its original TTL.
    // Never treat an old shared_ptr held by an effect as a new producer heartbeat.
    bool Usable() const;
};

class Source final
{
public:
    // Identical configs share one reader/worker via weak registry ownership.
    // Invalid config throws invalid_argument; no app or capture is auto-started.
    static std::shared_ptr<Source> Acquire(const Config& config);
    ~Source();
    Source(const Source&) = delete;
    Source& operator=(const Source&) = delete;
    Snapshot Read() const;
    Config Settings() const { return config; }

private:
    explicit Source(const Config& config);
    void Run();
    void Publish(State state, std::shared_ptr<const Frame> frame, std::string detail);
    const Config config;
    mutable std::mutex snapshot_mutex;
    Snapshot snapshot;
    std::mutex stop_mutex;
    std::condition_variable wake;
    bool stopping = false;
    std::thread worker;
};
}
