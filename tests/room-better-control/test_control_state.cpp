/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ScreenSources/BetterControlState.h"
#include <cstdlib>
#include <iostream>

using namespace better_source;
static unsigned checks = 0;
static void Check(bool value, const char* name)
{ ++checks; if(!value) { std::cerr << "FAILED: " << name << '\n'; std::exit(1); } }

int main()
{
    std::uint64_t number = 42;
    Check(ParseDecimal64("18446744073709551615", number) && number == UINT64_MAX, "uint64 maximum exact");
    Check(ParseDecimal64("9007199254740993", number) && number == UINT64_C(9007199254740993), "above JSON double precision exact");
    for(const std::string invalid : {"", "0", "-1", "+1", " 1", "1 ", "1.0", "1e3", "18446744073709551616", "999999999999999999999"})
    {
        number = 42;
        Check(!ParseDecimal64(invalid, number) && number == 42, "invalid integer leaves destination unchanged");
    }

    SurfaceStamp raw{"fixture-Raw", 9007199254740993ULL, 7, 800, 600, 3200};
    SurfaceStamp coverage{"fixture-Coverage", 18446744073709551615ULL, 1, 800, 600, 3200};
    FrameEnvelope env;
    env.raw = raw; env.raw.sequence = 1; env.coverage = coverage;
    env.state_revision = env.rendering_state_revision = 12;
    Check(ValidateFrameBinding(raw, env, coverage) == FrameBinding::Ready, "later raw shares immutable first-sequence state");
    auto raw_old = raw; --raw_old.generation;
    Check(ValidateFrameBinding(raw_old, env, coverage) == FrameBinding::RawChanged, "no old raw with current metadata");
    auto raw_low = raw; raw_low.sequence = 2; auto env_later = env; env_later.raw.sequence = 3;
    Check(ValidateFrameBinding(raw_low, env_later, coverage) == FrameBinding::RawBeforeState, "raw before publication refused");
    auto mask_new = coverage; --mask_new.generation;
    Check(ValidateFrameBinding(raw, env, mask_new) == FrameBinding::CoverageChanged, "new mask forces raw reread");
    mask_new = coverage; ++mask_new.sequence;
    Check(ValidateFrameBinding(raw, env, mask_new) == FrameBinding::CoverageChanged, "coverage sequence must be exact");
    auto wrong_channel = raw; wrong_channel.channel = "other-Raw";
    Check(ValidateFrameBinding(wrong_channel, env, coverage) == FrameBinding::RawChanged, "channel identity checked");
    auto bad = env; ++bad.rendering_state_revision;
    Check(ValidateFrameBinding(raw, bad, coverage) == FrameBinding::InvalidEnvelope, "render state must match envelope");
    bad = env; bad.raw_format = "RGBA8";
    Check(ValidateFrameBinding(raw, bad, coverage) == FrameBinding::InvalidEnvelope, "format rejected");
    bad = env; bad.rendering_version = 2;
    Check(ValidateFrameBinding(raw, bad, coverage) == FrameBinding::InvalidEnvelope, "future rendering schema rejected");
    bad = env; bad.coverage.width = 400;
    Check(ValidateFrameBinding(raw, bad, coverage) == FrameBinding::InvalidEnvelope, "raw mask geometry matches");
    bad = env; bad.coverage.channel = bad.raw.channel;
    Check(ValidateFrameBinding(raw, bad, coverage) == FrameBinding::InvalidEnvelope, "raw mask cannot alias one channel");
    auto invalid_stamp = raw; invalid_stamp.stride = UINT32_MAX;
    Check(!ValidStamp(invalid_stamp), "stride overflow bounded");
    invalid_stamp = raw; invalid_stamp.width = 16385;
    Check(!ValidStamp(invalid_stamp), "dimensions bounded");
    invalid_stamp = raw; invalid_stamp.channel = "Local\\injected";
    Check(!ValidStamp(invalid_stamp), "channel not a kernel path");

    SceneTarget target{"instance-a", 8, 3, "scene-one"};
    EffectiveScene effective{8, 3, "scene-one", false};
    Check(ObserveScene(target, "instance-a", 8, nullptr) == SceneResult::Pending, "202 without frame remains pending");
    Check(ObserveScene(target, "instance-a", 8, &effective) == SceneResult::Effective, "exact matching frame confirms scene");
    Check(ObserveScene(target, "instance-b", 8, &effective) == SceneResult::InstanceChanged, "restarted producer invalidates request");
    Check(ObserveScene(target, "instance-a", 9, &effective) == SceneResult::Superseded, "new control revision beats stale effective frame");
    effective.control_revision = 9;
    Check(ObserveScene(target, "instance-a", 8, &effective) == SceneResult::Superseded, "new effective revision is not earlier success");
    effective = {8, 3, "scene-one", true};
    Check(ObserveScene(target, "instance-a", 8, &effective) == SceneResult::Pending, "scene loading cannot confirm");
    effective = {8, 2, "scene-one", false};
    Check(ObserveScene(target, "instance-a", 8, &effective) == SceneResult::Pending, "scene revision must match");
    effective = {8, 3, "scene-other", false};
    Check(ObserveScene(target, "instance-a", 8, &effective) == SceneResult::Pending, "scene identity must match");
    effective = {8, 3, "", false}; target.scene_id.clear();
    Check(ObserveScene(target, "instance-a", 8, &effective) == SceneResult::Effective, "null previous scene valid after release");
    Check(ObserveScene(target, "instance-a", 7, &effective) == SceneResult::Pending, "older status does not complete command");
    target.control_revision = 0;
    Check(ObserveScene(target, "instance-a", 0, &effective) == SceneResult::Invalid, "uninitialized command invalid");

    SceneClaims claims;
    Check(claims.Empty() && claims.Epoch() == 0, "raw reads have no lease claim");
    Check(claims.Claim("effect-a", "scene-one") == SceneClaims::ClaimResult::Added, "first owner");
    const auto first_epoch = claims.Epoch();
    Check(claims.Claim("effect-a", "scene-one") == SceneClaims::ClaimResult::Unchanged && claims.Epoch() == first_epoch, "idempotent same effect");
    Check(claims.Claim("effect-b", "scene-one") == SceneClaims::ClaimResult::Added && claims.Epoch() == first_epoch, "compatible effect shares lease");
    Check(claims.Claim("effect-c", "scene-two") == SceneClaims::ClaimResult::Conflict && claims.Count() == 2, "different scene cannot steal lease");
    Check(claims.Claim("effect-a", "scene-two") == SceneClaims::ClaimResult::Conflict, "shared owner cannot change everyone scene");
    Check(claims.Release("missing") == SceneClaims::ReleaseResult::Unknown, "unknown owner release harmless");
    Check(claims.Release("effect-b") == SceneClaims::ReleaseResult::Remaining && claims.Scene() == "scene-one", "one stop retains shared lease");
    Check(claims.Claim("effect-a", "scene-two") == SceneClaims::ClaimResult::Changed && claims.Epoch() != first_epoch, "single owner scene switch invalidates queued request");
    Check(claims.Release("effect-a") == SceneClaims::ReleaseResult::Last && claims.Empty() && claims.Scene().empty(), "last stop requests remote restoration");
    Check(claims.Claim("", "scene") == SceneClaims::ClaimResult::Invalid, "empty owner refused");
    Check(claims.Claim("owner", "") == SceneClaims::ClaimResult::Invalid, "empty scene is raw mode outside claims");
    Check(claims.Claim("bad\nowner", "scene") == SceneClaims::ClaimResult::Invalid, "control characters refused");
    for(unsigned i = 0; i < 64; ++i) Check(claims.Claim("owner-" + std::to_string(i), "scene") == SceneClaims::ClaimResult::Added, "bounded owner insertion");
    Check(claims.Claim("overflow", "scene") == SceneClaims::ClaimResult::Full && claims.Count() == 64, "bounded owner count");
    std::cout << "Better native control policy: " << checks << " checks PASS\n";
}
