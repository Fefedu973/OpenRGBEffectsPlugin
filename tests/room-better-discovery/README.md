# Better native discovery and optional scene control

`ScreenSources/BetterDiscovery.{h,cpp}` implements authenticated discovery, generation metadata and optional temporary scene selection from Better's native integration v1. DTOs and control semantics are checked against `NativeApiServer.cs`, `NativeConnectionFile.cs` and `NativeControlService.cs` in BetterSignalRGBScreenCapture. No capture application is launched. Discovery alone remains read-only; a lease is acquired only after an explicit `RequestScene` call. Global appearance preferences are never changed.

`better_source::Discovery::Acquire(Config)` shares one worker per normalized descriptor path/timing pair. An empty path uses `%LOCALAPPDATA%/Better_SignalRGB_Screen_Capture/ApplicationData/NativeOutput/connection.json`; a test or custom profile may supply an explicit absolute path. Keep the provider only while an effect or its source panel is active. `Read()` returns an immutable `shared_ptr<const Snapshot>` without I/O; `Refresh()` schedules a bounded refresh. The GUI may poll it. Scene names are data and should be displayed as plain text.

The worker rereads the descriptor before each cycle, then authenticates GET `/api/native/v1/discovery` and `/api/native/v1/scenes`. It accepts only the exact version-one IPv4 listener form `http://127.0.0.1:<port>`, a 64-hex Bearer token, a nonzero UUID and a valid process ID. It bypasses proxies, disables redirects/cookie persistence, limits descriptor/response sizes to16KiB/256KiB, and checks instance identity, ORGBFRM1 version/header/capacity, channel roles and formats, scene UUIDs and duplicate IDs. A failed refresh clears ready channels instead of retaining a falsely live result. No URL, token or untrusted server error body is exposed in the snapshot or a log; credentials are held only in private request memory and never written.

The public states distinguish `MissingDescriptor`, `InvalidDescriptor`, `Unavailable`, `Unauthorized` and `Incompatible`; `Ready` means successful discovery, not a guarantee that a first raw frame already exists. The image reader must still enforce publication lifetime/TTL and, for appearance metadata, generation-bound association. A raw consumer needs no lease and must not disable Better's glow or edit global appearance.

Network I/O and its event loop run exclusively on a worker thread. Each request times out independently, and destruction aborts an in-flight request through the worker loop without a GUI callback or a detached thread.

## Generation metadata

`RequestState(generation)` schedules a latest-only authenticated GET of `/api/native/v1/states/<generation>`; `ReadState(generation)` returns an immutable cached result or null. The cache holds at most eight generations, clears on a discovered instance change and throttles repeated requests to 250ms. Generation values are parsed as exact decimal uint64 strings, including values beyond JSON's exact numeric range. A 404 remains retryable; mismatched generations and malformed envelopes are invalid.

`PublishedStateStatus::Ready` validates the requested generation and raw channel, but does not authorize pixels by itself. Consumers must match the instance and validate raw, immutable metadata and coverage through `BetterControlState`/`BetterFrameSource`. Holding an old shared snapshot remains safe after cache eviction.

## Temporary scene selection

`RequestScene(owner, scene_uuid)` records the latest intent for a unique local effect owner. Empty scene UUID releases that owner. Owners requesting the same scene share one lease; a different scene returns an owner-specific conflict. `ReleaseScene(owner)` leaves the lease active for remaining owners and releases it after the last owner stops. No bearer token or server lease capability is part of a public snapshot.

`ReadControl(owner)` distinguishes waiting, pending, effective, conflict, superseded, unavailable and releasing. A 202 response is pending until the exact scene and control revisions appear in a published image. Renewal also changes the control revision and must receive that acknowledgement; the client never renews while an acknowledgement is pending. Consumers must use exact revision equality, not accept an arbitrary higher revision, which can represent a manual override. New image generations with the same acknowledged scene/control revisions remain valid when their metadata is bound correctly.

The worker uses a 30-second lease and normally renews an effective lease after ten seconds. Changing a scene does not reset the renewal clock. Pending acknowledgement is bounded; a manual override, supersession or uncertain acquisition blocks automatic reacquisition for that intent. Explicit release/new intent, or a verified new Better instance, permits retry. A late acquisition response retains its capability before checking intent so a cancelled owner cannot leak a known lease.

Shutdown attempts to release an owned lease with a 750ms HTTP deadline. If a capability was never returned, the client cannot identify that lease for deletion; Better's 30-second expiry is the fallback. Cleanup is therefore bounded and best-effort, rather than a guarantee that a remote acknowledgement always arrives before destruction.

## Synthetic verification

From an x64 MSVC developer prompt:

```powershell
python tests/room-better-discovery/run.py --qt <Qt-root>
```

The harness compiles the actual worker against QtCore/QtNetwork. Its 167 checks create only temporary descriptors and synthetic loopback TCP listeners; it never reads the personal descriptor. Checks cover authenticated requests, immutable snapshots, sharing, URL/auth guards, proxy bypass, redirect non-forwarding, malformed/oversized responses, duplicate scenes, timeout/recovery, descriptor removal and bounded shutdown. Control cases include delayed 202 acknowledgement, exact uint64 generations, renewal, shared-owner release, scene changes, conflicts, supersession, late-acquisition cancellation and destructor release. Metadata cases cover cache eviction, immutable held results, 404 retries and wrong-generation rejection. No HTTP call to a personal Better service is part of the test.

The separate `tests/room-better-control/run_frames.py` suite also compiles this client with the real ORGBFRM1 reader and validates generation/coverage binding against a synthetic service.

Dependencies for integration: add the two source files and Qt Network to the existing Effects build. This helper does not change any SDK/plugin ABI.
