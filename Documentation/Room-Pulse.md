# Room Pulse: device-oriented music visualization

This original preset was designed after inspecting the installed Pump Up Beats
v5.2 effect and its saved settings. That effect organizes the canvas by device
response: a mirrored central spectrum, side and top meters, pulse tiles and a
fan sector. It is not three independent low/mid/high panels. No proprietary
JavaScript, graphics or other source is included in this repository.

The reference settings used thin bars, color changes on bass attacks, a nearly
black background, no background flash or trails, quick release and a spectrum
scaled by overall level. Room Pulse follows that visual intent with its own
GLSL renderer and a small native, spectrum-derived onset state. It does not
claim sample-identical output or equivalent frequency calibration.

## Placement contract

Output is 800×500; the design grid is 320×200, origin top-left. Use these bounds
in the Visual Map layout. Each route remains an ordinary generic LED or image
route, with no controller-name-dependent behavior in the shader.

| Region | Design bounds | Response |
|---|---|---|
| Main spectrum | X48–320, Y30–185 | Mirrored around (184,107.5), low frequencies at center, higher frequencies outward |
| Frequency line | X48–320, Y185–200 | Same horizontal spectrum expressed as brightness, suitable for long strips |
| Volume VU | X0–48, Y0–200 | Broadband level, fills bottom to top |
| Bass VU | X48–180, Y0–30 | Bass level, fills left to right |
| Bass tile | X180–220, Y0–30 | Uniform brightness from bass envelope |
| Volume tile | X220–260, Y0–30 | Uniform brightness from broadband envelope |
| Fan sector | X260–290, Y0–30 | Sector around (275,15), starts at bottom and sweeps clockwise |
| Ring support | X290–320, Y0–10 and Y20–30 | Steady color support for non-ring parts |
| Ring pulse | X290–320, Y10–20 | Bass brightness for the ring itself |

Place a full visual surface across the main spectrum to see its symmetry.
Long strips can use the frequency line or a VU. Lamps and small shapes should
use a tile; fan geometry can use the sector or the ring region. The region map
is deliberately different from the older Tri Band layout. Loading only the
shader on a Tri Band map will produce misplaced responses.

## Native audio state and controls

`iAudio[256]` still contains the existing filtered spectrum: 64 magnitudes,
each repeated four times. No FFT or capture transport has changed. The new
`iMusic` uniform contains `(broadband envelope, bass envelope, onset hue,
onset pulse)`. Broadband level is derived from the spectrum RMS, not a new
time-domain volume API. Bass uses three low non-DC magnitudes. Thus this is
musical band energy, not a calibrated sound-level meter.

Attack is immediate; volume and bass releases use time constants of 160 ms and
90 ms. A rising bass transient must exceed a recent-energy baseline and pass
a 140 ms retrigger guard before changing color. A constant tone or elapsed time
alone cannot change the onset hue. There is no BPM estimator or clock-driven
beat. Processing uses monotonic time independently of shader animation speed.
Missing audio clears energy immediately; a long pause resets stale history.

The existing audio amplitude, normalization, smoothing, decay and equalizer
controls still apply upstream. Shader constants are grouped at the top of
`shaders/room-pulse.fs`: color style (static/wave/onset), static color,
background level, volume/spectrum gains, volume scaling, visible magnitudes,
thin-bar width, bottom-line height and hue-wave speed. Default onset color is
shared across the room; no permanent red/cyan/yellow column palette is used.
The steady ring-support regions intentionally remain lit during silence; other
regions return to the configured nearly black background.

## Prepare a profile

```text
python tools/write-music-profile.py --identity-template existing-canvas-shader.json --preset room-pulse --audio-device 2147483647 --out room-pulse-draft.json
```

The result targets `Music - Room Pulse.json`, uses CustomName `Room Pulse` and
has AutoStart disabled. The caller must provide the matching layout and verify
its identity. A template's saved Visual Map selector is updated consistently.
No application or device is contacted. Output files are never overwritten.

Unlike the earlier asset-only spectrum revision, Room Pulse needs the updated
Effects DLL that supplies `iMusic`. All older shaders continue to receive the
same existing uniforms and need no changes. Null audio now explicitly clears
the GPU `iAudio` array instead of retaining its previous values.

## Validation

```text
python -m unittest discover -s tests/room-audio -p test_music_profile.py -v
python tests/room-audio/run.py --qt QT_ROOT --openrgb-root OPENRGB_ROOT --room-pulse --profile room-pulse-draft.json --output build/room-audio/room-pulse
```

The tests exercise the real envelope helper and production ShaderProgram and
ShaderPass in a real offscreen GL context. They verify silence, constant tones,
separated attacks, retrigger suppression, frame-rate-independent release,
missing/non-finite input, and the exact spatial regions with synthetic audio.
They also test the integrated helper-to-shader path and clearing prior GPU data
on disconnect. No valid audio endpoint is registered and no lighting hardware
is opened. Runtime listening and perceived physical response remain separate
integration checks.
