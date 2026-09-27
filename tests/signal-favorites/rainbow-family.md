# Rainbow / gradient family

Nine original GLSL 1.30 fragment implementations and 55 source-compatible controls.
This folder contains declared metadata and mathematical checks only. It does not
redistribute the inspected JavaScript, browser assets or private user preferences.
The source IDs and hashes in `rainbow-family.metadata.json` identify the versions
whose controls were inspected; they do not imply a redistribution license.

Run `python tests/signal-favorites/rainbow-family.py`. Optionally pass `--inventory`
with a private local metadata inventory to compare every label, range, default,
enum option and source hash against the inspected declarations. Compilation and
real OpenGL rendering are separate checks; this Python suite does not claim them.
`rainbow-family.cases.json` provides 17 independent color checkpoints for the GL
test host, including unsorted/coincident stops, degenerate gradients, alternating
sectors, source orientation, directional sweeps and breathing extrema.

The shaders normalize each output to a 320 × 200 reference rectangle and invert
OpenGL Y once. Positions and scale controls keep their original units. The output
resolution can change without changing those controls. The engine supplies color
uniforms as RGB in [0,1], enums as option indices and `t_<key>` as the integral of
each numeric parameter with time. No shader multiplies integrated speed by speed
again; setting speed to zero freezes its current phase.

## Temporal and visual boundaries

Only Spiral Rainbow used elapsed time in the inspected source. The remaining
animated implementations use a 60 Hz reference to translate increments per frame
to elapsed seconds. This is a declared calibration, not a measurement of the old
SignalRGB browser cadence. Phase does not jump when speed changes. Switching
reverse/direction or changing spatial controls can recompose the field. The
breathing and hue-cycle clocks continue while their toggles are disabled because
the common engine exposes unconditional numeric integrals.

- Spiral Rainbow uses an angular spectrum; Custom Spiral uses 8, 6 or 8 sectors
  for 2, 3 or 4 colors. Analytic borders replace overlapping browser wedges.
- Gradient preserves stop order at ties, sorts positions implicitly and extends
  endpoint colors. Identical endpoints produce black. The old frame is not retained.
- Rainbow Rise keeps its origin, radial scale and 360-reference-pixel extent.
  Continuous sampling replaces individually stroked and accumulated circles.
- Gradient Wave keeps direction, repeat stops, scaling and HSL hue cycling.
  Browser HSL quantization and frame-count wrap overshoot are not reproduced.
- Rainbow Tunnel has 49 visible rings plus the zero-radius reference ring. Radius
  spacing is adjustable while stroke width stays 30 reference pixels. The original
  never cleared previous strokes; this stateless shader does not reproduce that
  history-dependent trail. Its fixed 49-iteration loop needs GPU timing in the
  integrated renderer; no performance result is asserted here.
- Rainbow uses a continuous spectrum instead of 320/200 one-pixel strips.
- Side to Side preserves the sequence and width/height-dependent sweep duration.
  Optional rainbow hues are deterministic per sweep, not the old random sequence.
- Solid Color multiplies RGB by the breathing alpha directly and avoids the old
  intermediate integer HSL conversion.

## Real OpenGL result, 27 September 2026

The integrated `render_favorites.cpp` test uses the production ShaderProgram and
ShaderPass, a real offscreen OpenGL context and an 800 × 500 output. The first run
containing this family compiled/rendered all 12 then-present presets, exercised
every declared control at its boundaries and passed all **17/17** independent
color checkpoints. The contact sheet was inspected. Evidence is generated in
`build/signal-favorites/gpu-validation.json` and `contact-sheet.png`; these local
render outputs are not source fixtures.

Warm CPU wall time for Draw plus synchronous FBO image readback, six samples per
preset (not a GPU-only timer, device frame rate or endurance result):

| Preset | Average ms | Maximum ms |
|---|---:|---:|
| Spiral Rainbow | 1.097 | 1.220 |
| Custom Spiral | 1.107 | 1.438 |
| Gradient | 1.098 | 1.214 |
| Rainbow Rise | 1.154 | 1.426 |
| Gradient Wave | 1.205 | 1.511 |
| Rainbow Tunnel | 1.063 | 1.202 |
| Rainbow | 0.965 | 1.088 |
| Side to Side | 0.985 | 1.191 |
| Solid Color | 1.087 | 1.209 |

The initial animation probe compared t=1 and t=4 seconds. Custom Spiral at its
default speed rotates 900 degrees in that interval, exactly five repetitions of
its 180-degree visual period, so those two images match despite animation. A
noncommensurate second timestamp is required when asserting frame differences.

No port is represented as pixel-perfect or optically validated. The known
differences above remain in each preset's notes. The GL checkpoints validate
specific mathematical colors and boundaries, not a full browser-to-shader image
comparison or confirmation on physical lighting.
