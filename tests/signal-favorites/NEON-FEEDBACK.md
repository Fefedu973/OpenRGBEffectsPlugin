# Neon Nebula and native feedback

`run_neon.py --qt <Qt-root> --openrgb-root <OpenRGB-root>` builds the real
`ShaderPass.cpp` and `ShaderProgram.cpp` with MSVC and runs them in a local
offscreen OpenGL context. It does not open an RGB device or capture input.

The test verifies 247 assertions, including successive feedback values read
from the GPU, reset on compile/resize/restart, old profile compatibility,
serialization/copy, disabled feedback, the latest output sampled by a subsequent
pass, eight control boundaries and tap positions/expiry/speed continuity.
The renderer outputs a preview and JSON timing report to ignored
`build/neon-feedback`. The test at 800×500 drew and read back 60 frames at a mean
2.18 ms on the development machine. This is a local draw/readback measurement,
not physical device FPS or a guarantee for another GPU.

`neon_metadata.py` checks the eight original control declarations. Its optional
`--reference <effect.html>` reads the installed source in place and verifies its
audited SHA-256 before comparing the actual HTML metadata. No proprietary source
is redistributed. Audited source ID: `-Mjb1GEcNJhwbQxjTw-z`, publisher Alex
Krastev, SHA-256
`0e7eab87de0209e8c9128571cdd6fc25d2d960313a3e287d0f472299121a1f71`.

The new GLSL preserves persistent source-over composition, the three waves,
phase-swapped palette, six particle colors and expanding tap rings. It uses a
bounded deterministic 300-particle field rather than the random source list;
startup population, mature particle radii and antialiasing differ. Integrated
speed is calibrated to the reference at 60 Hz. Accumulation occurs once per
rendered frame, so opacity remains frame-rate dependent, as in the reference.
Pixel-for-pixel equivalence is not claimed.

Feedback is an optional `ShaderPassData.feedback` boolean (legacy default false),
saved in shader profile JSON and preserved by copying. A feedback buffer owns
two RGBA FBOs within the existing 4096-dimension / 8-megapixel canvas limit:
at most 64 MiB of color storage for the pair. The previous completed frame is
sampled on texture unit 5 and the alternate FBO receives the next frame. No
texture is simultaneously sampled and rendered. Units 0–3 remain shader
channels and unit 4 the final output. Resize allocation preserves existing
channel bindings, including the first frame of a multipass program.

Both history surfaces clear to transparent black on initialization, resize,
recompile and restart; cleanup deletes both on the renderer's GL thread. A
non-feedback pass retains one FBO. Neon uses transparent initial history to
paint its one-time teal base, then emits opaque output. The original source's
unused crack routine is intentionally absent.
