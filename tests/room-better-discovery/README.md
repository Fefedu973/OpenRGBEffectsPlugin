# Better native discovery

`ScreenSources/BetterDiscovery.{h,cpp}` implements the descriptor and two read-only HTTP calls specified by Better's native integration v1. The DTOs are checked against `NativeApiServer.cs` and `NativeConnectionFile.cs` in BetterSignalRGBScreenCapture. No capture application is launched, no scene or preference is changed, and discovery does not acquire a control lease.

`better_source::Discovery::Acquire(Config)` shares one worker per normalized descriptor path/timing pair. An empty path uses `%LOCALAPPDATA%/Better_SignalRGB_Screen_Capture/ApplicationData/NativeOutput/connection.json`; a test or custom profile may supply an explicit absolute path. Keep the provider only while an effect or its source panel is active. `Read()` returns an immutable `shared_ptr<const Snapshot>` without I/O; `Refresh()` schedules a bounded refresh. The GUI may poll it. Scene names are data and should be displayed as plain text.

The worker rereads the descriptor before each cycle, then authenticates GET `/api/native/v1/discovery` and `/api/native/v1/scenes`. It accepts only the exact version-one IPv4 listener form `http://127.0.0.1:<port>`, a 64-hex Bearer token, a nonzero UUID and a valid process ID. It bypasses proxies, disables redirects/cookie persistence, limits descriptor/response sizes to16KiB/256KiB, and checks instance identity, ORGBFRM1 version/header/capacity, channel roles and formats, scene UUIDs and duplicate IDs. A failed refresh clears ready channels instead of retaining a falsely live result. No URL, token or untrusted server error body is exposed in the snapshot or a log; credentials are held only in private request memory and never written.

The public states distinguish `MissingDescriptor`, `InvalidDescriptor`, `Unavailable`, `Unauthorized` and `Incompatible`; `Ready` means successful discovery, not a guarantee that a first raw frame already exists. The image reader must still enforce publication lifetime/TTL and, for appearance metadata, generation-bound association. A raw consumer needs no lease and must not disable Better's glow or edit global appearance.

Network I/O and its event loop run exclusively on a worker thread. Each request times out independently, and destruction aborts an in-flight request through the worker loop without a GUI callback or a detached thread.

## Synthetic verification

From an x64 MSVC developer prompt:

```powershell
python tests/room-better-discovery/run.py --qt <Qt-root>
```

The harness compiles the actual worker against QtCore/QtNetwork. It creates only temporary descriptors and synthetic loopback TCP listeners; it never reads the personal descriptor. It verifies authenticated requests, immutable snapshots, sharing, all URL/auth guards, proxy bypass, redirect non-forwarding, malformed/oversized responses, duplicate scenes, timeout/recovery, descriptor removal and bounded shutdown during a pending response. No HTTP call to a personal Better service is part of the test.

Dependencies for integration: add the two source files and Qt Network to the existing Effects build. This helper does not change any SDK/plugin ABI.
