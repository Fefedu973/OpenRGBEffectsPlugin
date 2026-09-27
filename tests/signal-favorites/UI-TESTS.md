# Native favorite UI and persistence checks

`run_ui.py` loads the production Effects DLL using `QPluginLoader` and an API 5
host with no controllers. It never starts an effect, opens an audio capture, or
accesses hardware. The real Qt widgets, preset resources, profile load/save
methods and ordinary Shaders implementation run unchanged.

From an x64 MSVC developer shell:

```powershell
python tests/signal-favorites/run_ui.py --qt <Qt-msvc-x64> --jom <jom.exe> --core <OpenRGB-Room> --dll <built-Effects.dll>
```

The test discovers every preset embedded in the DLL and checks that all appear
in the **SignalRGB Favorites** menu. It loads, saves and reloads each class with
`AutoStart=false`, checks that shipped shader code is not embedded in the saved
profile, and verifies the controls survive a profile round trip. Further cases
exercise invalid values, numeric bounds, direct UI edits, canvas size, preview
persistence, the editable shader program of the ordinary Shaders effect, and
its opt-in rhythm checkbox without starting audio.

Validation on 2026-09-27: **247 assertions passed for 13 native presets**, using the
real release DLL and Qt 6.8.3 offscreen, including Galaxies. The count grows when additional presets
are embedded. The test also caught an invalid-color fallback that changed case
on the next save/load; the fallback now uses the same canonical QColor format
as valid inputs.

This is UI and persistence validation. GPU rendering and animation behavior are
covered separately by `render_favorites.cpp` / `run.py`; this test makes no
physical-device or visual-fidelity claim.
