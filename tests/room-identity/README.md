The fixture compiles the current `ControllerZone::matches_json` and `to_json`
method bodies, extracted without changes, with an in-memory controller. No
OpenRGB instance or hardware is opened.

From an x64 MSVC developer prompt:

```powershell
python tests/room-identity/run.py --openrgb-root ../OpenRGB-Room
python tests/room-identity/run.py --openrgb-root ../OpenRGB-Room --without-fix
```

The first run checks updates from saved plugin/firmware versions, retained
version metadata, mismatching identity fields, zones and segments, and the
existing HID/I2C location policy. The second reinstates the former version
comparison in the extracted method and must reproduce the lost selection.

Version is diagnostic metadata, not a stable identity field. Name, serial,
description, vendor, zone, segment and the existing location rules still decide
the match. This does not introduce a name-only or count-based fallback.
