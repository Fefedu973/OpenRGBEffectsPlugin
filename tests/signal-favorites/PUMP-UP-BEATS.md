# Native Pump Up Beats

This is an original C++/GLSL implementation of the installed version 5.2 visual
design by Julian Lang, source ID `-MhwiL3cQgnjhqzS0g2L`. No original Canvas
source is distributed. Its audited SHA-256 is
`9b0d5159d7ef5fa6a75ae709a676bd76b5cc37d6a3ceb2d2c697dc4d29a9ddc9`.
The twenty control names, labels, defaults, limits and enum values are retained.
It is distinct from the earlier original Room Pulse visualizer.

## Rendering and layout

The canvas uses 320×200 design coordinates, top-left origin; native image output
can still be 800×500 or another bounded resolution. At the default frequency
size170 the left VU is x0–48, the top strip y0–30, bass bar x48–180, bass rectangle
x180–220, volume rectangle x220–260, fan square x260–290, and three-part Wraith
display x290–320. The symmetric spectrum occupies x48–320 and y30–196; the last
four rows carry frequency brightness. Changing frequency size moves these same
regions; it does not redefine any device routes.

Six spectrum modes, five color modes, independently decaying volume/bass
envelopes, beat-color/background choices and the pausable helper are native.
Helper simulation takes ten seconds per cycle and is paused by a new keyboard
tap or preview click. RandomBeat is driven by recent low-frequency energy and
variance, with a100ms refractory; it is not a predicted BPM clock.

Fading uses the optional ping-pong GPU history. It affects the spectrum, VU,
bass bar and fan, while the brightness rectangles and bottom line are cleared
each frame. The reference's unusual bottom HueWave background overlay (alpha
squared) is preserved, including a black full-strength line on a black
background. Static bars alternate their two colors; Smooth uses their gradient.
Fan gradient wedges use fifty quantized angular steps. GPU edge antialiasing,
the small slice-overlap approximation and seeded random hue selection can
produce pixel differences. A restart, resize or recompile clears all history.

## Audio contract

`RhythmSnapshot` also publishes mean-square mono power and two hundred linear
FFT amplitude samples at 0,50,…9950Hz. These come from the already-computed
continuous Hann-window FFT; they are not repeated legacy64-bin values and do
not require a second FFT, additional capture thread or retained PCM recording.
Above-Nyquist samples are zero. Missing packets clear both fields; the snapshot
is copied under the existing session mutex. Native amplitude calibration is
explicit, but equivalence to undocumented SignalRGB byte/level scaling is not
claimed. Volume Boost remains available to adjust the displayed level.

`native_pump::State::Update(parameters,elapsed,snapshot,helperTap)` returns
`Frame.levels` (volume bar/rectangle, bass bar/rectangle), `Frame.state` (cyclic
hue, random hue, flash, helper phase), and `Frame.frequencies[100]`. It performs
bounded scalar operations, not CPU rasterization. The shader receives those as
`pumpLevels`, `pumpState`, and `pumpFreq[100]`. The containing native Favorite
owns Audio settings and publishes normalized UI parameters separately.

## Reproducible checks

From an x64 MSVC developer shell:

```powershell
python tests/signal-favorites/run_pump.py --qt <Qt-root> --openrgb-root <core-repository>
python tests/signal-favorites/pump_metadata.py --reference <installed-effect.html>
python tests/room-audio/run_capture.py
python tests/room-rhythm/run.py
```

The production ShaderProgram/ShaderPass GPU harness checks twenty controls,
52 boundary cases, six distinct geometries, known per-region pixel gains,
feedback decay and reset at800×500. The independent PCM/state harness checks
44.1/48kHz fragmented tones, exact spectral peak indices, low-rate Nyquist
bounds, silence, volume/bass decay, helper pause, actual beat hue changes and
discontinuity. Capture tests additionally compare all200 bins after stereo and
7.1 packet decoding, and verify idle clearing. Neither harness controls LEDs
or opens a recording stream. Draw/readback timings are not device FPS claims.
