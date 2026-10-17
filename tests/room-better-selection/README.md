# Better source selection lifecycle

This harness compiles the production `ScreenSourceSelection` QWidget and its real
Qt signals, JSON settings, activation/reconfiguration logic and frame gates. Strict
I/O-boundary doubles record owner claims and provide synthetic immutable frames.
The discovery worker's HTTP, lease expiry and refcount arbitration are tested
separately by `tests/room-better-discovery`; this suite tests the widget's calls into it.

Run from an MSVC developer shell with the project's Qt version:

```powershell
python tests/room-better-selection/run.py --qt <Qt MSVC directory> --core <OpenRGB-Room directory>
```

The runner forces Qt's `offscreen` platform. It opens no desktop window, connects
to no HTTP server, performs no screen capture, and reads/writes no personal profile.
Only a temporary build under `build/better-selection-tests` is produced.

Checks cover default/migrated opt-out settings, stopped/visible configuration,
actual `SetRunning` activation, two independent effect owners, stop/destruction,
appearance and size changes without lease churn, visible manual-override status,
and paired-frame gating when an existing connection gains/loses capture capability.
Capture-only images must be at least as new as the capture acknowledgement, while later
canvas/appearance revisions are allowed; explicit scene claims still require exact frame
association. Instance identity, expiry and manual revocation remain enforced.

Validated on 2026-09-28 with Qt 6.8.3/MSVC x64: **40 assertions passed**. This is
selection/lifecycle coverage with test doubles, not an HTTP provider, plugin installation,
real capture or physical LED acceptance test.
