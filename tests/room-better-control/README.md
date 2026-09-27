# Better native API v1 policy

`python tests/room-better-control/run_tests.py` compiles the pure production
`ScreenSources/BetterControlState.h` with MSVC C++17. It opens no API, frame
mapping, capture source or hardware device.

The synthetic tests cover decimal uint64 values above JavaScript precision,
strict raw-generation/coverage-sequence pairing, immutable first-frame state,
dimension/stride limits, exact scene acknowledgement, supersession, producer
restart, and bounded local sharing/conflicts between effects.

These helpers do not claim that a frame was displayed. `Ready` validates only
the identity and geometry of copies already read through the bounded ORGBFRM1
reader. The caller must validate freshness and buffer integrity separately.

## Worker integration

- Raw capture effects never acquire a scene lease. A requested saved scene is
  one global Better state. `SceneClaims` shares a lease for identical requests
  and rejects incompatible requests; it is confined to one provider worker.
- Normalize scene UUIDs before `Claim`. Opaque local owner IDs are not tokens.
  On `Added`, acquire only if no remote lease exists. On `Changed`, select the
  new scene using that lease. `ReleaseResult::Last` requests remote release and
  restoration. Keep remote ownership until DELETE resolves or its TTL expires.
- `Epoch()` identifies current local intent, not remote lease ownership. Even
  a stale acquire reply may contain a lease capability that must be cleaned up.
  Serialize acquire/select/release and do not drop capabilities with an old
  response. No lease IDs or Bearer tokens belong in settings or logs.
- A 202 response remains pending. Store its exact target control/scene revision
  and identity, then compare the effective state via `ObserveScene`. Never
  accept a higher revision as completion. HTTP 409 `state_superseded` ends that
  request. Instance changes invalidate all pending commands and remote leases.
- Renew changes Better's control revision too. Avoid renew while waiting for a
  scene ACK; use a 30-second lease and renew at about 10 seconds. Respect manual
  overrides and expired/conflicting leases instead of silently reacquiring.
- For appearance, read raw, fetch/cache `states/<generation>`, then read the
  envelope's exact coverage generation and sequence. If either channel moved,
  restart from the latest raw. Cache metadata by `(instance_id, generation)`.
  The state endpoint's raw sequence is the first sequence, so newer raw frames
  in the same generation are valid. Never pair old raw with current `/status`.

Appearance processing is separate from this policy and from the artistic
Screen Ambience ports. Raw is opaque black-composited pixels; coverage is a
grayscale opacity image. Raw alpha and black pixel tests are not coverage.
Better's complete global filters/placement/halo are not implemented by this
header or by the three SignalRGB-style screen effects.

## Paired frame worker integration test

From an x64 MSVC developer shell:

```text
python tests/room-better-control/run_frames.py --qt <Qt-msvc-root> --core <OpenRGB-Room-root>
```

This compiles actual `BetterDiscovery`, `BetterFrameSource` and `ScreenSource`,
not transport mocks. A temporary descriptor, synthetic authenticated loopback
HTTP server, and actual ORGBFRM1 publishers provide 800×600 frames. No installed
Better descriptor, capture source, scene or hardware is used.

48 checks pass: missing producer/state, delayed 404 recovery, first-sequence
metadata reuse, immutable images, BGRA/coverage identity, coverage-first
generation transition, stale data, non-grayscale masks, inconsistent rendering
revision, instance replacement and bounded shutdown with HTTP pending. No HTTP
mutation is emitted by the paired provider.

`better_source::FrameSource::Acquire(discovery)` shares one passive worker per
Discovery instance. `Read()` returns `shared_ptr<const FrameSnapshot>` containing
`raw`, `coverage`, `metadata` (the rendering object), `instance_id`, exact raw and
coverage generation/sequence, scene/control revisions and `expires`. It performs
no network, mapping or pixel work. A Ready snapshot must still pass `Usable()`
when retained across render frames; neither a held snapshot nor polling alone
extends its expiry. Non-ready snapshots expose no renderable images. Pixel data
uses Qt implicit sharing, so snapshot copies do not copy a complete image.

The worker delegates authenticated state fetches to Discovery's bounded cache,
and bounds its own parsed cache to eight generations. Invalid metadata fails
closed; renderer-specific validation of source polygons and settings remains in
the appearance renderer. Coverage grayscale validation runs once per mask
generation/sequence. Full appearance rendering and visible equivalence are
outside this integration test.
