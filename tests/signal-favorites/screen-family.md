# Native screen effects

These are three different native implementations, with the original artistic
control keys, labels, limits, defaults and enum order. They require the shared
native screen-source provider and dynamic shader textures. The tests use only
synthetic images; they do not open a monitor capture, device, keyboard listener
or external bridge.

## Data and render contract

- `native_screen::State::Update(id, parameters, dt, image, sequence, taps)`
  returns immutable numeric RGBA32F data, dimensions, sequence and uniforms.
  `Declarations(id)` supplies only the state uniforms. The renderer owns GL.
- The capture is top-left oriented. Dynamic `iChannel0` carries the raw image;
  `iChannel1` carries the numeric state. Global `iScreenAvailable` and
  `iScreenResolution` come from the renderer. Unavailable input renders black.
- Reduction uses the entire selected source rectangle, stretched to 28×20 and
  160×100 with Qt smooth reduction. There is no additional crop, letterbox or
  selection of another monitor. Source selection is shared infrastructure.
- Only a new sequence or size repeats source reduction. Numeric state advances
  on every render step, including repeated stationary captures. Reset on effect
  start and source/channel/generation changes; do not reset on artistic edits.
- `dt` is intentionally unused for these per-render histories: the reference
  effects update per animation callback rather than by elapsed seconds.
- The two HD histories allocate on the heap; Reset clears them in place. No
  large temporary is placed on the render worker's Windows stack.

## Effect behavior

**Average Color** computes the arithmetic mean of hue, saturation and lightness
across 560 cells. This intentionally differs from RGB averaging and circular
hue averaging. Tap Effect multiplies the global gain by 0.95 per event, followed
by 0.025 recovery per render. The shader draws a uniform color; it does not
invent spatial ripples.

**Screen Ambience** keeps separate Float32 histories for HSL cells and HD RGBA.
First input initializes directly. Standard hue smoothing follows the short arc;
Dominant retains the reference's last eligible cell and non-circular one-degree
hue movement. Mono/Cinema/Vivid operate on HSL saturation. The GPU then applies
RGB hue-rotation, brightness, saturation and contrast, in that order. Two
separable Gaussian convolutions follow. Pass slots are:

| Slot | Role |
| --- | --- |
| 0 | Raw dynamic image |
| 1 | Numeric 28×20 HSL or 160×100 RGBA |
| 2 | `screen-ambience-color.fs`: mode and RGB filters |
| 3 | `screen-ambience-blur-x.fs`: horizontal blur |
| Main | `screen-ambience.fs`: vertical blur and output |

Blur is measured in logical 320×200 units, independently scaled to the output
axes. The kernel is normalized and bounded to three sigma (radius at most 60).
Samples outside the image contribute transparent black, not edge clamping.
Blur zero bypasses convolution. Intermediate texture sizes are queried rather
than assumed equal to the output size.

**LSD Ambience** draws 560 ordered circles at their original asymmetric cell
centers. Their radii and growth flags persist; waves change radius with the
observed stationary newest wave and forward-removal skips. The two additions
of color-cycle hue are intentional. Screen/Custom/Cycle/Gradient remain distinct;
Custom changes hue while retaining saturation/lightness derived from the screen.
The GLSL shader visits only cells within the maximum current radius and preserves
row-major paint order. Run-length wave storage retains repeated zero-speed
births without allocating one entry per birth. A defensive cap of 8192 distinct
groups drops the oldest only for pathological fractional-speed histories beyond
normal integer UI stepping; `State::Overflowed()` exposes that limit.

## Validation and limits

Run `run_screen.py --qt <Qt-root> --openrgb-root <core-root>` inside an MSVC x64
environment. It runs metadata checks, native state tests and the real production
`ShaderPass`/`ShaderProgram` on an offscreen OpenGL context at 800×500.

Covered cases include HSL versus RGB averaging, tap recovery, first-frame
initialization, hue wrapping, frozen-source convergence, independent HD history,
ties-to-even HD quantization, Dominant selection, all control extrema, missing
source, four-corner orientation in Standard and HD, filter extreme values,
transparent blur boundaries, circle positions and Custom versus Screen colors.
Returned frame storage remains immutable after subsequent state updates.

An additional private reference comparison simulated 750 LSD frames across
speed-zero births, restart/recycling and color/radius/fade changes. All 44 sampled
full-cell snapshots (2240 values each) matched the locally read reference to
within 1.98e-6, consistent with float upload conversion. The reference HTML is
read in memory by that private harness and is not included in this repository.

This establishes the tested state behavior, not pixel identity with SignalRGB:
the proprietary `engine.zone` reduction is unavailable, Qt reduction and explicit
HD bilinear stretch may differ, Canvas edge coverage differs from GLSL, and the
finite three-sigma Gaussian is a documented approximation to CSS blur. Shader
tests use mathematical expectations, not captured user screens. Output timings
include numeric updates, upload, GPU passes and synchronous QImage readback;
they do not represent device FPS or GPU-only timings. Hardware and real capture
validation are separate deployment steps.
