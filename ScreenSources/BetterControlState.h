/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

// Pure policy for Better native API v1. No HTTP, credentials, clock, Qt, or
// producer access. The provider calls this on its single serialized worker.
namespace better_source
{
inline bool ParseDecimal64(const std::string& text, std::uint64_t& value)
{
    if(text.empty() || text.size() > 20) return false;
    std::uint64_t parsed = 0;
    for(char ch : text)
    {
        if(ch < '0' || ch > '9') return false;
        const unsigned digit = static_cast<unsigned>(ch - '0');
        if(parsed > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) return false;
        parsed = parsed * 10 + digit;
    }
    if(parsed == 0) return false;
    value = parsed;
    return true;
}

struct SurfaceStamp
{
    std::string channel;
    std::uint64_t generation = 0, sequence = 0;
    std::uint32_t width = 0, height = 0, stride = 0;
};

inline bool ValidStamp(const SurfaceStamp& stamp)
{
    // ORGBFRM1 allocation and overflow bounds; the producer currently emits
    // smaller 320x200/800x600 images, but this validator is not resolution-bound.
    if(stamp.channel.empty() || stamp.channel.size() > 64 ||
       !stamp.generation || !stamp.sequence || !stamp.width || !stamp.height ||
       stamp.width > 16384 || stamp.height > 16384) return false;
    for(char c : stamp.channel)
        if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
             (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return stamp.stride >= static_cast<std::uint64_t>(stamp.width) * 4 &&
           static_cast<std::uint64_t>(stamp.stride) * stamp.height <= 64ULL * 1024 * 1024;
}

inline bool SameGeometry(const SurfaceStamp& a, const SurfaceStamp& b)
{ return a.width == b.width && a.height == b.height && a.stride == b.stride; }

struct FrameEnvelope
{
    SurfaceStamp raw, coverage;
    std::uint64_t state_revision = 0, rendering_state_revision = 0;
    unsigned rendering_version = 1;
    std::string rendering_schema = "better.native-rendering";
    std::string raw_output_id = "canvas-raw", coverage_output_id = "canvas-coverage";
    std::string raw_format = "BGRA8_OPAQUE_SRGB", coverage_format = "BGRA8_OPAQUE_SRGB";
};

enum class FrameBinding { Ready, InvalidEnvelope, RawChanged, RawBeforeState, CoverageChanged };

// The envelope is GET states/<raw generation>, not mutable GET status metadata.
// Returned Ready means identity/geometry pairing only: readers must separately
// validate ORGBFRM1 integrity, TTL and image buffer lengths before calling.
inline FrameBinding ValidateFrameBinding(const SurfaceStamp& raw,
                                         const FrameEnvelope& envelope,
                                         const SurfaceStamp& coverage)
{
    if(!ValidStamp(raw) || !ValidStamp(coverage) ||
       !ValidStamp(envelope.raw) || !ValidStamp(envelope.coverage) ||
       !envelope.state_revision || envelope.state_revision != envelope.rendering_state_revision ||
       envelope.rendering_version != 1 || envelope.rendering_schema != "better.native-rendering" ||
       envelope.raw_output_id != "canvas-raw" || envelope.coverage_output_id != "canvas-coverage" ||
       envelope.raw_format != "BGRA8_OPAQUE_SRGB" || envelope.coverage_format != "BGRA8_OPAQUE_SRGB" ||
       envelope.raw.channel == envelope.coverage.channel || !SameGeometry(envelope.raw, envelope.coverage))
        return FrameBinding::InvalidEnvelope;
    if(raw.channel != envelope.raw.channel || raw.generation != envelope.raw.generation ||
       !SameGeometry(raw, envelope.raw)) return FrameBinding::RawChanged;
    if(raw.sequence < envelope.raw.sequence) return FrameBinding::RawBeforeState;
    if(coverage.channel != envelope.coverage.channel || coverage.generation != envelope.coverage.generation ||
       coverage.sequence != envelope.coverage.sequence || !SameGeometry(coverage, envelope.coverage))
        return FrameBinding::CoverageChanged;
    return FrameBinding::Ready;
}

struct SceneTarget
{
    std::string instance_id;
    std::uint64_t control_revision = 0, scene_revision = 0;
    // Empty is the API's null scene identity, valid e.g. after release.
    std::string scene_id;
};

struct EffectiveScene
{
    std::uint64_t control_revision = 0, scene_revision = 0;
    std::string scene_id;
    bool scene_loading = false;
};

enum class SceneResult { Pending, Effective, Superseded, InstanceChanged, Invalid };

// A higher revision is NOT completion of an earlier command. Call for a 200
// effective ACK or a status poll after 202 pending_frame. An HTTP 409
// state_superseded is terminal and maps directly to Superseded.
inline SceneResult ObserveScene(const SceneTarget& target, const std::string& instance_id,
                                std::uint64_t current_control_revision,
                                const EffectiveScene* effective)
{
    if(target.instance_id.empty() || !target.control_revision) return SceneResult::Invalid;
    if(instance_id != target.instance_id) return SceneResult::InstanceChanged;
    if(current_control_revision > target.control_revision ||
       (effective && effective->control_revision > target.control_revision)) return SceneResult::Superseded;
    if(!effective || current_control_revision < target.control_revision) return SceneResult::Pending;
    if(effective->control_revision == target.control_revision &&
       effective->scene_revision == target.scene_revision && effective->scene_id == target.scene_id &&
       !effective->scene_loading) return SceneResult::Effective;
    return SceneResult::Pending;
}

// One Better instance owns one global scene. Compatible effect requests share
// a single provider lease; raw readers never call Claim. This registry stores
// local opaque owner IDs and desired scene IDs, never the Bearer or lease UUID.
// HTTP acquire/delete must still be serialized: an old acquire response may
// contain a capability that needs releasing even after Epoch() has changed.
class SceneClaims
{
public:
    enum class ClaimResult { Added, Unchanged, Changed, Conflict, Invalid, Full };
    enum class ReleaseResult { Unknown, Remaining, Last };

    ClaimResult Claim(const std::string& owner, const std::string& scene)
    {
        if(!ValidId(owner) || !ValidId(scene)) return ClaimResult::Invalid;
        for(auto& item : claims)
            if(item == owner)
            {
                if(scene == desired_scene) return ClaimResult::Unchanged;
                if(claims.size() != 1) return ClaimResult::Conflict;
                desired_scene = scene; ++epoch;
                return ClaimResult::Changed;
            }
        if(!claims.empty() && desired_scene != scene) return ClaimResult::Conflict;
        if(claims.size() >= 64) return ClaimResult::Full;
        if(claims.empty()) { desired_scene = scene; ++epoch; }
        claims.push_back(owner);
        return ClaimResult::Added;
    }

    ReleaseResult Release(const std::string& owner)
    {
        for(auto it = claims.begin(); it != claims.end(); ++it)
            if(*it == owner)
            {
                claims.erase(it);
                if(!claims.empty()) return ReleaseResult::Remaining;
                desired_scene.clear(); ++epoch;
                return ReleaseResult::Last;
            }
        return ReleaseResult::Unknown;
    }

    bool Empty() const { return claims.empty(); }
    std::size_t Count() const { return claims.size(); }
    const std::string& Scene() const { return desired_scene; }
    std::uint64_t Epoch() const { return epoch; }

private:
    static bool ValidId(const std::string& id)
    {
        if(id.empty() || id.size() > 128) return false;
        for(unsigned char c : id) if(c < 32 || c == 127) return false;
        return true;
    }
    std::vector<std::string> claims;
    std::string desired_scene;
    std::uint64_t epoch = 0;
};
}
