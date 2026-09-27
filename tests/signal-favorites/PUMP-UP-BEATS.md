# Native Pump Up Beats

This is an original C++/GLSL implementation of the installed version 5.2 visual
design by Julian Lang, source ID `-MhwiL3cQgnjhqzS0g2L`. No original Canvas
source is distributed. Its audited SHA-256 is
`9b0d5159d7ef5fa6a75ae709a676bd76b5cc37d6a3ceb2d2c697dc4d29a9ddc9`.
The twenty control names, labels, defaults and limits are retained. The original
enum values retain their indices; `ScreenDominant` is appended to Color Style.
It is distinct from the earlier original Room Pulse visualizer.

## Optional screen palette

`ScreenDominant` selects the most populous **chromatic** color family in the
selected screen source. It is opt-in: the default remains HueCycle. The source
selector is shown and capture runs only while this mode and the effect are
active. Native Windows desktop capture needs no external program. Optional
Better/FrameSurface inputs reuse the existing input provider; Better itself
must be running for its source to be available. Pump always consumes raw pixels,
never the Better appearance render graph, and keeps its own spectrum/regions.

A uniform nearest-pixel grid (at most 128×72 samples) feeds 24 circular hue bins
at most 10 times/s. Pixels require HSV value ≥0.08, saturation ≥0.25 and chroma
≥0.05; dark padding, grays and very faint color noise do not win. Each candidate
family combines its bin and both neighbors, including red across 359°/0°.
Alpha weights population; source brightness does not weight it. The winner
must cover at least 0.5% of opaque sampled area and eight sampled pixels (one
for an input with fewer than eight opaque samples). Its circular mean hue and
mean saturation are preserved; HSV value is normalized to one.

Missing/expired, transparent, entirely black or neutral input uses **Static
Color 1**, rather than keeping an old screen color. A neutral new image replaces
the previous palette within the 100 ms reduction interval; missing/expired
input resets it immediately. The source color therefore no longer darkens the
audio response a second time: Pump's existing audio levels control foreground
brightness. Choosing a dark Static Color 1 still explicitly gives a dark
fallback. This music-oriented palette is deliberately different from copying
the darkest/largest screen background; it is not object recognition.
Quantization, sample coverage and ties can affect the selected family; ties
are deterministic. No arbitrary saturation boost is applied.

Audio gains, silence behavior, backgrounds and the twenty controls are otherwise
unchanged. The source choice is saved with the profile. Tests use synthetic
images (padding, neutral apps, photos represented by color patches, hue wrap,
varying exposure, alpha and disappearance) and a local offscreen GPU. No real
desktop capture is performed by these tests.

Live diagnosis on 2026-09-27 separately confirmed the failure on a colorful
Better raw frame: 43.84% of its canvas was black placement padding. The former
RGB4 vote returned approximately (0.47, 0.19, 0.36) in RGB8 units; this hue-family
reducer returned approximately (255, 160, 36) from the same frame. Its selected
chromatic family covered 16.7% of all samples. A separate neutral-window frame
correctly produced the configured static fallback. Private captures remain
local and are not part of the repository or test fixtures.

## Rendering and layout

The canvas uses 320×200 design coordinates, top-left origin; native image output
can still be 800×500 or another bounded resolution. At the default frequency
size170 the left VU is x0–48, the top strip y0–30, bass bar x48–180, bass rectangle
x180–220, volume rectangle x220–260, fan square x260–290, and three-part Wraith
display x290–320. The symmetric spectrum occupies x48–320 and y30–196; the last
four rows carry frequency brightness. Changing frequency size moves these same
regions; it does not redefine any device routes.

Six spectrum modes, five original color modes plus ScreenDominant, independently decaying volume/bass
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

The native effect is registered as `SignalFavorite.PumpUpBeats`. A profile that
still selects generic `Shaders` with the embedded `room-pulse.fs` is the older
Room Pulse visualizer, even when both profiles use the same music layout.
When migrating an existing room profile, retain its current ControllerZones and
Visual Map identity; an older Pump template can omit devices added subsequently.
The profile name may stay unchanged so existing menu shortcuts keep working.

`Volume Boost` controls native power scaling, independently of the legacy FFT
`amplitude` control. The instantaneous volume is
`min(1, 2.4 * mean_square_pcm * 6^((boost-50)/20))`, followed by the selected
decay. For example, RMS 0.125 gives about 3.8% at boost50 or 35.2% at boost75,
before decay. Raising Windows volume cannot fix a wrong effect selection or
guarantee that different capture calibrations match. Calibrate this control on
the selected output with actual music; the same slider value as SignalRGB does
not establish equivalent loudness. Spectrum geometry and audio calibration
must be checked separately.

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
53 boundary cases, six distinct geometries, known per-region pixel gains,
feedback decay and reset at800×500. The independent PCM/state harness checks
44.1/48kHz fragmented tones, exact spectral peak indices, low-rate Nyquist
bounds, silence, volume/bass decay, helper pause, actual beat hue changes and
discontinuity. Capture tests additionally compare all200 bins after stereo and
7.1 packet decoding, and verify idle clearing. Neither harness controls LEDs
or opens a recording stream. Draw/readback timings are not device FPS claims.
