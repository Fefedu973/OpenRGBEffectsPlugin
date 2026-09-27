# Native capture sources for shader effects

Screen Ambience, Average Color and LSD Ambience now share an image input and
the existing canvas/device output routing. Their **Screen source** controls
select a native display or **BetterScreenCapture — native frames**. No WebView,
virtual LED matrix, external bridge process or second capture application is
introduced by this integration.

The selected source and channel are saved in each effect profile. Native
capture starts only when the effect starts. Better mode subscribes to an
existing publisher; it does not launch Better, fall back to another display,
change a scene or modify the application's settings. An absent, invalid or
expired publisher produces black, with a source status visible in the UI.

## Better application contract

The default channel is `better-screen-capture`, editable per effect. The
Windows transport is the core's unchanged `FrameSurface/FrameSurface.h`
**ORGBFRM1** contract. Publish opaque BGRA8 sRGB, top-left first, bounded to
64 MiB. The shader input accepts dimensions up to 4096 on either axis, with the
same byte limit. Respect the named mutex, owner lifetime and generation fields.

Publish a static-image heartbeat at least every 500 ms, incrementing sequence,
for the default 2000 ms TTL. A reader-held image does not count as a heartbeat.
Restart with a new generation; shutdown should close the owner lifetime.

Matching source configurations share a single worker and latest-frame mailbox.
It uses the actual core Reader, validates stride/dimensions/alpha, and transfers
owned pixels into an immutable QImage. Effects never read mapped memory or
perform transport I/O in their rendering step. See
[the reader contract](../ScreenSources/README.md).

**Application integration remains separate.** The audited Better 1.3.0 did not
yet publish this transport. This consumer has been tested with synthetic native
publishers, including the real wire-to-GPU path; that does not demonstrate a
connection to the running Better application. Its existing MJPEG endpoints are
not consumed by this implementation.

Scene selection, authenticated control leases and coverage/appearance metadata
will be connected after the Better-side API is implemented and agreed. There
are no guessed endpoints or nonfunctional scene controls. Raw Better frames
already omit its web halo; requesting raw input does not require disabling the
user's persisted glow setting.

## Renderer and effect integration

`DynamicShaderImage` is an immutable RGBA8 image or RGBA32F numeric texture,
with sequence, generation, local source revision and expiration. Input texel
(0,0) is the **top-left**, without an implicit vertical flip. Numeric textures
use nearest sampling; color images use linear sampling and clamp to edge.

`ShaderRenderer::UpdateInputs` replaces custom uniforms and all image slots
atomically, preventing a source frame from being paired with the previous
effect-state texture. GL upload happens only on the renderer thread and only
for changed input; storage is reused until size/format changes. Expiration is
also checked at draw time, so a delayed effect step cannot renew a dead source.
This still involves CPU pixel copies/conversion and a GPU upload, not zero-copy.

`ShaderPass::DYNAMIC_IMAGE` selects one of four immutable input slots. BUFFER
passes can optionally specify bounded fixed dimensions; otherwise they follow
the canvas. Presets declare `screenReactive: true` and up to two extra shader
passes. The preset loader supplies dynamic raw input on channel 0 and numeric
effect state on channel 1, then the declared buffers on channels 2 and 3.
`iScreenAvailable` and `iScreenResolution` describe the raw input.

The shared screen state performs numeric reduction/history, while every output
pixel is drawn in GLSL. It resets on restart, producer generation or source
selection changes; artistic controls retain their intended history. Ordinary
non-screen presets do not allocate screen history buffers. Separate per-effect
state avoids cross-talk between concurrent effects sharing one input.

Screen Ambience preserves seven artistic controls and separate Standard/HD
histories; Average Color averages HSL and supports keyboard taps; LSD Ambience
keeps its circle/wave state. Browser filtering and antialiasing differences are
documented in [screen effect validation](../tests/signal-favorites/screen-family.md).
No pixel-perfect claim is made.

## Validation

- `tests/room-screen-sources`: real Writer/Reader, stride, immutable ownership,
  latest frame, multiple subscribers, TTL, contention, producer death and
  reconnection; also builds the unavailable-backend branch.
- `tests/room-shaders/run_dynamic.py`: actual OpenGL uploads, four-corner BGRA
  orientation, unchanged-frame upload counts, RGBA32F, expiration, recovery,
  resize, fixed-size multipass rendering and synthetic wire-to-GPU input.
- `tests/signal-favorites/run_screen.py`: numerical state and production GPU
  checks, source/control boundaries, synthetic orientation and temporal cases.
- The combined favorite GPU and real DLL/UI suites verify every registered
  preset and persist source selection without starting capture or hardware.

The native DLL is staged separately before replacing a running installation.
Build/test success and synthetic latency measurements are not physical-device
frame-rate or optical-parity validation.
