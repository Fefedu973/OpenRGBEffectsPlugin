/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BetterFrameSource.h"
#include "BetterControlState.h"
#include "ScreenSource.h"
#include <QJsonArray>
#include <algorithm>
#include <condition_variable>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace better_source
{
namespace
{
using Clock = std::chrono::steady_clock;
bool Integer(const QJsonValue& value, std::uint64_t& result, bool allow_zero = false)
{
    const qint64 number = value.toInteger(-1);
    if(!value.isDouble() || number < 0 || (!allow_zero && number == 0)) return false;
    result = static_cast<std::uint64_t>(number);
    return true;
}
bool Surface(const QJsonValue& value, SurfaceStamp& stamp, std::string& id, std::string& format)
{
    if(!value.isObject()) return false;
    const auto object = value.toObject();
    if(!object.value("generation").isString() || !object.value("sequence").isString() ||
       !ParseDecimal64(object.value("generation").toString().toStdString(), stamp.generation) ||
       !ParseDecimal64(object.value("sequence").toString().toStdString(), stamp.sequence)) return false;
    std::uint64_t width, height, stride;
    if(!Integer(object.value("width"), width) || !Integer(object.value("height"), height) ||
       !Integer(object.value("stride"), stride) || width > UINT32_MAX || height > UINT32_MAX || stride > UINT32_MAX) return false;
    stamp.channel = object.value("channel").toString().toStdString();
    stamp.width = static_cast<std::uint32_t>(width); stamp.height = static_cast<std::uint32_t>(height);
    stamp.stride = static_cast<std::uint32_t>(stride);
    id = object.value("outputId").toString().toStdString(); format = object.value("format").toString().toStdString();
    return ValidStamp(stamp);
}
struct ParsedState
{
    FrameEnvelope envelope;
    QJsonObject metadata;
    std::uint64_t control_revision = 0, scene_revision = 0;
    QString scene_id;
};
bool ParseState(const QJsonObject& json, const Snapshot& discovery, ParsedState& parsed)
{
    auto& env = parsed.envelope;
    if(!Integer(json.value("stateRevision"), env.state_revision) ||
       !Integer(json.value("controlRevision"), parsed.control_revision, true) ||
       !Surface(json.value("image"), env.raw, env.raw_output_id, env.raw_format) ||
       !Surface(json.value("coverage"), env.coverage, env.coverage_output_id, env.coverage_format) ||
       env.raw.channel != discovery.raw_channel.toStdString() ||
       env.coverage.channel != discovery.coverage_channel.toStdString() ||
       !json.value("rendering").isObject() || !json.value("scene").isObject()) return false;
    parsed.metadata = json.value("rendering").toObject();
    const auto& rendering = parsed.metadata;
    std::uint64_t version, width, height, canvas_width, canvas_height;
    if(!Integer(rendering.value("version"), version) || version != 1 ||
       !Integer(rendering.value("stateRevision"), env.rendering_state_revision) ||
       !Integer(rendering.value("outputWidth"), width) || !Integer(rendering.value("outputHeight"), height) ||
       !Integer(rendering.value("canvasWidth"), canvas_width) || !Integer(rendering.value("canvasHeight"), canvas_height) ||
       width != env.raw.width || height != env.raw.height ||
       canvas_width != discovery.canvas_width || canvas_height != discovery.canvas_height ||
       !rendering.value("effectiveSettings").isObject() || !rendering.value("sources").isArray() ||
       rendering.value("sources").toArray().size() > 128) return false;
    env.rendering_version = static_cast<unsigned>(version);
    env.rendering_schema = rendering.value("schema").toString().toStdString();
    const auto scene = json.value("scene").toObject();
    if(!Integer(scene.value("stateRevision"), parsed.scene_revision, true)) return false;
    const auto scene_id = scene.value("activeSceneId");
    if(!scene_id.isNull() && !scene_id.isString()) return false;
    parsed.scene_id = scene_id.toString();
    if(parsed.scene_id.size() > 128) return false;
    // Validate all envelope properties even before a matching coverage arrives.
    return ValidateFrameBinding(env.raw, env, env.coverage) == FrameBinding::Ready;
}
SurfaceStamp Stamp(const screen_source::Frame& frame, const QString& channel)
{
    return {channel.toStdString(), frame.generation, frame.sequence,
            static_cast<std::uint32_t>(frame.image.width()), static_cast<std::uint32_t>(frame.image.height()),
            static_cast<std::uint32_t>(frame.image.bytesPerLine())};
}
bool Grayscale(const QImage& image)
{
    if(image.format() != QImage::Format_ARGB32) return false;
    for(int y = 0; y < image.height(); ++y)
    {
        const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for(int x = 0; x < image.width(); ++x)
            if(qRed(row[x]) != qGreen(row[x]) || qRed(row[x]) != qBlue(row[x]) || qAlpha(row[x]) != 255) return false;
    }
    return true;
}
}

const char* FrameStateName(FrameState state)
{
    switch(state)
    {
    case FrameState::Starting:return "starting"; case FrameState::Ready:return "ready";
    case FrameState::WaitingDiscovery:return "waiting_discovery"; case FrameState::WaitingRaw:return "waiting_raw";
    case FrameState::WaitingState:return "waiting_state"; case FrameState::WaitingCoverage:return "waiting_coverage";
    case FrameState::Invalid:return "invalid"; case FrameState::Stale:return "stale"; case FrameState::Stopped:return "stopped";
    }
    return "invalid";
}
bool FrameSnapshot::Usable() const
{ return Ready() && !raw.isNull() && !coverage.isNull() && Clock::now() <= expires; }

class FrameSource::Impl final
{
public:
    explicit Impl(std::shared_ptr<Discovery> value):discovery(std::move(value)),snapshot(std::make_shared<const FrameSnapshot>())
    { worker = std::thread(&Impl::Run, this); }
    ~Impl()
    {
        { std::lock_guard<std::mutex> lock(stop_mutex); stopping = true; }
        wake.notify_all(); if(worker.joinable()) worker.join();
    }
    std::shared_ptr<const FrameSnapshot> Read() const
    {
        std::lock_guard<std::mutex> lock(snapshot_mutex);
        if(snapshot->Ready() && Clock::now() > snapshot->expires)
        {
            auto stale = std::make_shared<FrameSnapshot>();
            stale->state = FrameState::Stale; stale->detail = "Bound frame TTL expired"; stale->revision = snapshot->revision;
            return stale;
        }
        return snapshot;
    }
private:
    std::shared_ptr<Discovery> discovery;
    mutable std::mutex snapshot_mutex;
    std::shared_ptr<const FrameSnapshot> snapshot;
    std::mutex stop_mutex;
    std::condition_variable wake;
    bool stopping = false;
    std::thread worker;
    std::shared_ptr<screen_source::Source> raw_source, coverage_source;
    QString instance, raw_channel, coverage_channel;
    unsigned ttl = 0;
    std::map<std::uint64_t, ParsedState> state_cache;
    std::uint64_t checked_mask_generation = 0, checked_mask_sequence = 0;
    bool mask_valid = false;

    void Publish(FrameSnapshot next)
    {
        std::lock_guard<std::mutex> lock(snapshot_mutex);
        if(next.state == snapshot->state && next.detail == snapshot->detail && next.instance_id == snapshot->instance_id &&
           next.raw_generation == snapshot->raw_generation && next.raw_sequence == snapshot->raw_sequence &&
           next.coverage_generation == snapshot->coverage_generation && next.coverage_sequence == snapshot->coverage_sequence &&
           next.expires == snapshot->expires) return;
        next.revision = snapshot->revision + 1;
        snapshot = std::make_shared<const FrameSnapshot>(std::move(next));
    }
    void Fail(FrameState state, const char* detail)
    { FrameSnapshot next; next.state = state; next.detail = QString::fromLatin1(detail); Publish(std::move(next)); }
    void ResetSources()
    {
        raw_source.reset(); coverage_source.reset(); state_cache.clear();
        checked_mask_generation = checked_mask_sequence = 0; mask_valid = false;
        instance.clear(); raw_channel.clear(); coverage_channel.clear(); ttl = 0;
    }
    void Tick()
    {
        const auto current = discovery->Read();
        if(!current || !current->Ready())
        { ResetSources(); Fail(FrameState::WaitingDiscovery, "Better discovery is not ready"); return; }
        if(instance != current->instance_id || raw_channel != current->raw_channel || coverage_channel != current->coverage_channel || ttl != current->recommended_ttl_ms)
        {
            ResetSources();
            instance = current->instance_id; raw_channel = current->raw_channel; coverage_channel = current->coverage_channel;
            ttl = current->recommended_ttl_ms;
            screen_source::Config config; config.channel = raw_channel.toStdString(); config.ttl_ms = ttl;
            raw_source = screen_source::Source::Acquire(config);
            config.channel = coverage_channel.toStdString(); coverage_source = screen_source::Source::Acquire(config);
        }
        const auto raw = raw_source->Read();
        if(!raw.Usable()) { Fail(FrameState::WaitingRaw, "Waiting for a fresh raw frame"); return; }
        auto found = state_cache.find(raw.frame->generation);
        if(found == state_cache.end())
        {
            discovery->RequestState(raw.frame->generation);
            const auto state = discovery->ReadState(raw.frame->generation);
            if(!state || state->status != PublishedStateStatus::Ready || state->instance_id != instance || state->requested_generation != raw.frame->generation)
            { Fail(FrameState::WaitingState, "Waiting for the exact raw generation metadata"); return; }
            ParsedState parsed;
            if(!ParseState(state->envelope, *current, parsed) || parsed.envelope.raw.generation != raw.frame->generation)
            { Fail(FrameState::Invalid, "Invalid raw generation metadata"); return; }
            // Bounded local cache; generation is opaque, not ordered by creation.
            if(state_cache.size() >= 8) state_cache.clear();
            found = state_cache.emplace(raw.frame->generation, std::move(parsed)).first;
        }
        const auto coverage = coverage_source->Read();
        if(!coverage.Usable()) { Fail(FrameState::WaitingCoverage, "Waiting for fresh coverage"); return; }
        const auto match = ValidateFrameBinding(Stamp(*raw.frame, raw_channel), found->second.envelope, Stamp(*coverage.frame, coverage_channel));
        if(match != FrameBinding::Ready)
        {
            Fail(match == FrameBinding::InvalidEnvelope ? FrameState::Invalid : FrameState::WaitingCoverage,
                 "Raw and coverage do not belong to the same published state"); return;
        }
        if(checked_mask_generation != coverage.frame->generation || checked_mask_sequence != coverage.frame->sequence)
        {
            mask_valid = Grayscale(coverage.frame->image);
            checked_mask_generation = coverage.frame->generation; checked_mask_sequence = coverage.frame->sequence;
        }
        if(!mask_valid) { Fail(FrameState::Invalid, "Coverage is not opaque grayscale"); return; }
        // Discovery may have changed while reading/parsing. Never publish a pair
        // tagged with a producer instance that has already been replaced.
        const auto latest = discovery->Read();
        if(!latest || !latest->Ready() || latest->instance_id != instance || latest->raw_channel != raw_channel || latest->coverage_channel != coverage_channel)
        { ResetSources(); Fail(FrameState::WaitingDiscovery, "Better instance changed during frame pairing"); return; }
        FrameSnapshot next; next.state = FrameState::Ready; next.instance_id = instance;
        next.raw = raw.frame->image; next.coverage = coverage.frame->image; next.metadata = found->second.metadata;
        next.raw_generation = raw.frame->generation; next.raw_sequence = raw.frame->sequence;
        next.coverage_generation = coverage.frame->generation; next.coverage_sequence = coverage.frame->sequence;
        next.state_revision = found->second.envelope.state_revision; next.control_revision = found->second.control_revision;
        next.scene_revision = found->second.scene_revision; next.scene_id = found->second.scene_id;
        next.expires = std::min(raw.frame->expires, coverage.frame->expires);
        if(Clock::now() > next.expires) { Fail(FrameState::Stale, "Bound frame TTL expired"); return; }
        Publish(std::move(next));
    }
    void Run()
    {
        for(;;)
        {
            { std::lock_guard<std::mutex> lock(stop_mutex); if(stopping) break; }
            try { Tick(); }
            catch(const std::exception&) { ResetSources(); Fail(FrameState::Invalid, "Better frame binding failed"); }
            std::unique_lock<std::mutex> lock(stop_mutex);
            if(wake.wait_for(lock, std::chrono::milliseconds(16), [this]{ return stopping; })) break;
        }
        ResetSources(); Fail(FrameState::Stopped, "stopped");
    }
};

std::shared_ptr<FrameSource> FrameSource::Acquire(std::shared_ptr<Discovery> discovery)
{
    if(!discovery) throw std::invalid_argument("Better frame source requires discovery");
    static std::mutex registry_mutex;
    static std::map<Discovery*, std::weak_ptr<FrameSource>> registry;
    std::lock_guard<std::mutex> lock(registry_mutex);
    for(auto i = registry.begin(); i != registry.end();) if(i->second.expired()) i = registry.erase(i); else ++i;
    if(auto found = registry[discovery.get()].lock()) return found;
    if(registry.size() > 16) throw std::runtime_error("Too many Better frame sources");
    auto value = std::shared_ptr<FrameSource>(new FrameSource(discovery)); registry[discovery.get()] = value; return value;
}
FrameSource::FrameSource(std::shared_ptr<Discovery> discovery):impl(new Impl(std::move(discovery))){}
FrameSource::~FrameSource() = default;
std::shared_ptr<const FrameSnapshot> FrameSource::Read() const { return impl->Read(); }
}
