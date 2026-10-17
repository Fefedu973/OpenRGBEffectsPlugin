# Better native discovery and optional scene control

`ScreenSources/BetterDiscovery.{h,cpp}` implements authenticated discovery, generation metadata, optional temporary scene selection and opt-in capture from Better's native integration v1. DTOs and control semantics are checked against `NativeApiServer.cs`, `NativeConnectionFile.cs` and `NativeControlService.cs` in BetterSignalRGBScreenCapture. No capture application is launched. Discovery and `BetterFrameSource` alone remain read-only; only explicit `RequestScene` or supported `RequestCapture` interests acquire a lease. Global appearance preferences are never changed.

`better_source::Discovery::Acquire(Config)` shares one worker per normalized descriptor path/timing pair. An empty path uses `%LOCALAPPDATA%/Better_SignalRGB_Screen_Capture/ApplicationData/NativeOutput/connection.json`; a test or custom profile may supply an explicit absolute path. Keep the provider only while an effect or its source panel is active. `Read()` returns an immutable `shared_ptr<const Snapshot>` without I/O; `Refresh()` schedules a bounded refresh. The GUI may poll it. Scene names are data and should be displayed as plain text.

The worker rereads the descriptor before each cycle, then authenticates GET `/api/native/v1/discovery` and `/api/native/v1/scenes`. It accepts only the exact version-one IPv4 listener form `http://127.0.0.1:<port>`, a 64-hex Bearer token, a nonzero UUID and a valid process ID. It bypasses proxies, disables redirects/cookie persistence, limits descriptor/response sizes to16KiB/256KiB, and checks instance identity, ORGBFRM1 version/header/capacity, channel roles and formats, scene UUIDs and duplicate IDs. A failed refresh clears ready channels instead of retaining a falsely live result. No URL, token or untrusted server error body is exposed in the snapshot or a log; credentials are held only in private request memory and never written.

The public states distinguish `MissingDescriptor`, `InvalidDescriptor`, `Unavailable`, `Unauthorized` and `Incompatible`; `Ready` means successful discovery, not a guarantee that a first raw frame already exists. The image reader must still enforce publication lifetime/TTL and, for appearance metadata, generation-bound association. A raw consumer needs no lease and must not disable Better's glow or edit global appearance.

Network I/O and its event loop run exclusively on a worker thread. Each request times out independently, and destruction aborts an in-flight request through the worker loop without a GUI callback or a detached thread.

## Generation metadata

`RequestState(generation)` schedules a latest-only authenticated GET of `/api/native/v1/states/<generation>`; `ReadState(generation)` returns an immutable cached result or null. The cache holds at most eight generations, clears on a discovered instance change and throttles repeated requests to 250ms. Generation values are parsed as exact decimal uint64 strings, including values beyond JSON's exact numeric range. A 404 remains retryable; mismatched generations and malformed envelopes are invalid.

`PublishedStateStatus::Ready` validates the requested generation and raw channel, but does not authorize pixels by itself. Consumers must match the instance and validate raw, immutable metadata and coverage through `BetterControlState`/`BetterFrameSource`. Holding an old shared snapshot remains safe after cache eviction.

## Temporary scene selection

`RequestScene(owner, scene_uuid)` records the latest scene intent for a unique local effect owner. Empty scene UUID releases that owner's scene interest. Owners requesting the same scene share one lease; a different scene returns an owner-specific conflict. `ReleaseScene(owner)` preserves capture interests; `ReleaseOwner(owner)` removes both. No bearer token or server lease capability is part of a public snapshot.

`ReadControl(owner)` distinguishes waiting, pending, effective, conflict, superseded, unavailable and releasing. A scene acknowledgement remains pending until its exact scene and control revisions appear in a published image. Scene-only leases do not renew while their acknowledgement is pending. Capture ownership is separate: it renews while waiting for images, and appearance/canvas revisions do not revoke it. Consumers selecting a scene still check exact revisions; capture-only readers instead require the fixed `minimum_capture_revision` from the successful start acknowledgement, the current instance and a fresh correctly paired frame.

Scene-only control uses a 30-second lease and normally renews an effective lease after ten seconds. Changing a scene does not reset the renewal clock. Pending scene acknowledgement is bounded; a manual override, supersession or uncertain acquisition blocks automatic reacquisition for that intent. Explicitly changing or removing the affected intent permits retry. A late acquisition response retains its capability before checking intent so a cancelled owner cannot leak a known lease.

Shutdown attempts to release an owned lease with a 750ms HTTP deadline. If a capability was never returned, the client cannot identify that lease for deletion; Better's lease expiry is the fallback. This is normally 30 seconds, or up to 300 seconds during capture startup. A startup already running in Better may finish before deferred restoration can acquire its lifecycle gate. Cleanup is bounded and best-effort, not a guarantee of immediate remote cancellation.

## Capture on demand

`RequestCapture(owner, true)` opts an active effect into capture; `false` removes only that owner's capture interest. The UI checkbox **Start Better capture while this effect runs** defaults off and persists as `capture_on_demand`. Merely opening the settings panel or selecting a source sends no capture command. Without the advertised `captureControl` capability, this option reports unsupported and consumes existing frames passively.

All scene and capture interests share one lease. Startup temporarily uses a 300-second lease because device preparation can exceed the usual 30 seconds. A successful `PUT capture {recording:true}` acknowledgement is followed by renewal down to 30 seconds, then renewal every 10 seconds even without a first image. Capture requests have a bounded 60-second deadline; ordinary reads keep their configured deadline, and scene mutations allow at least 3 seconds for Better's 2-second frame acknowledgement. Response loss never triggers a blind second start: the worker observes `isCaptureChanging` before attempting owned cleanup.

Removing the final interest sends **DELETE only**, restoring the producer's baseline rather than issuing `capture:false`. If capture interests disappear while a scene remains, release/reacquire restores recording before acquiring a scene-only lease. The inverse restores the original scene before reacquiring capture-only. Existing user recording or pause is preserved. For capture exclusively on demand, the user must turn off Better's **Auto-start recording on launch** preference; the provider never changes it.

`captureControlRevoked` records manual playback overrides independently of canvas, appearance and transient source transitions. A revoked capture interest requires a real opt-out/opt-in or effect restart; selecting another scene does not silently resume capture. A manually superseded scene stays revoked across transport release, including subsequent capture-only acquisition, until the explicit scene intent changes or is removed. Failures and conflicts are surfaced instead of repeatedly fighting user state. `capture_active` alone does not promise image availability; use `minimum_capture_revision`, frame TTL and generation/coverage association.

## Synthetic verification

From an x64 MSVC developer prompt:

```powershell
python tests/room-better-discovery/run.py --qt <Qt-root>
```

The harness compiles the actual worker against QtCore/QtNetwork. Both suites create only temporary descriptors and synthetic loopback TCP listeners; they never read the personal descriptor. The discovery suite covers authenticated requests, immutable snapshots, sharing, URL/auth guards, proxy bypass, redirect non-forwarding, malformed/oversized responses, duplicate scenes, timeout/recovery, descriptor removal and bounded shutdown. Control cases include delayed 202 acknowledgement, exact uint64 generations, renewal, shared-owner release, scene changes, conflicts, supersession, late-acquisition cancellation and destructor release. Metadata cases cover cache eviction, immutable held results, 404 retries and wrong-generation rejection.

The capture suite covers capability fallback, delayed acquisition/start, shared owners, startup/steady lease durations, more than 25 seconds without a first image, transition/appearance/canvas changes, manual playback and scene revocation, mixed-interest restoration, lost responses and owner departure during startup. No HTTP call to a personal Better service is part of either test.

The separate `tests/room-better-control/run_frames.py` suite also compiles this client with the real ORGBFRM1 reader and validates generation/coverage binding against a synthetic service.

Dependencies for integration: add the two source files and Qt Network to the existing Effects build. This helper does not change any SDK/plugin ABI.
