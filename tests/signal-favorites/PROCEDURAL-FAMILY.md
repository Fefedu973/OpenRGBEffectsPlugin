# Native procedural family

Five original GLSL implementations: **Visor, Custom Wave, Pinwheel, Spin and Plasma**. All 43 control keys, labels, defaults, choices and limits match the installed references recorded in `procedural-family.metadata.json`. No HTML, JavaScript, image asset or browser runtime is included.

## Behavior and fidelity boundaries

| Effect | Preserved behavior | Explicit difference |
|---|---|---|
| Visor | Separate axis positions, initial off-canvas position, overshoot/reversal, collision colors, background and source-over trails | Seeded valid RGB choices replace browser randomness. Motion uses a 60 Hz reference; trails accumulate per rendered frame. |
| Custom Wave | 50-unit stripes, exact palette order for 2/3/4 colors, orientation, count-specific wrapping and overshoot frame | Motion uses a 60 Hz reference. |
| Pinwheel | 37 ordered rays including the duplicated 260-degree ray; fixed endpoint circle; alternate Y center in edge mode; all four independent edge checks; radius-five hub | Native analytic antialiasing differs from Canvas. Negative rotation wraps at an equivalent full turn to avoid float growth. |
| Spin | 599 original polyline segments, ten turns, gap, butt ends, miter joins, reverse, speed denominator and hue rounding | Native analytic antialiasing differs from Canvas. A conservative radial bound skips only segments too far away to contribute. |
| Plasma | Cell size and gap, Fill/Fill All/Line alpha masks, HSL rounding, foreground/background timing, 11 gradient stops and gradient wrap | An original, deterministic integer hash selects the same 12 gradient directions instead of a randomly seeded permutation table. This is a different random field realization; it is not a pixel-identical replay. |

All drawing uses the 320 × 200 reference coordinate system, resampled by the GPU at the selected canvas resolution. Browser requestAnimationFrame timing is normalized to 60 Hz for the four discrete animations. Catch-up is bounded to 15 ticks after an ordinary update; an initial state tick is separate. Spin uses elapsed milliseconds directly. No renderer is claimed pixel-perfect or physically validated on LEDs in this batch.

**Fire and Ice is deferred.** Its source updates an in-place 320 × 50 stochastic heat grid. It cannot honestly be counted as another procedural flame; a separate stateful implementation and tests are needed.

## Integration contract

Add the five preset/shader pairs to the resource catalogue. Add `Effects/SignalFavorites/ProceduralEffectState.h` to the build and keep one `native_procedural::State` per effect instance. Merge `Update(id, parameters, elapsed_seconds)` into the custom uniform map each frame, and call `Reset()` on a new effect run. IDs are `Visor`, `CustomWave`, `Pinwheel`, `Spin`, and `Plasma`.

The helper owns only bounded numeric animation state. It returns `prState` and, for Visor, `prColor`. Each shader declares those uniforms; `Declarations()` intentionally returns an empty string. Visor alone sets `feedback: true`, using the existing separate read/write feedback textures. Restart, resize and recompile clear that history. No changes to the SDK, renderer thread model or device drivers are required.

## Verification

From an x64 MSVC developer prompt:

```powershell
python tests/signal-favorites/procedural_metadata.py
python tests/signal-favorites/run_procedural.py --qt <Qt-root> --openrgb-root <OpenRGB-root>
```

For a private source audit, optionally pass `--cache <SignalRGB-effects-cache>` to the metadata test. It verifies the five SHA-256 fingerprints and parses the control declarations without copying the sources.

The GPU harness compiles the **production ShaderProgram and ShaderPass**, renders at 800 × 500, tests every control boundary/enum option, checks state transitions and zero speed, exercises feedback decay/reset, and verifies Plasma grid/gap/HSL/mask behavior. Spin is compared to an independent QPainter polyline with butt ends and miter joins across 27 gap/width/angle combinations; stable interiors must match exactly, with no tolerance for missing geometry.

Verified locally: **2,333 GPU/state assertions, 88 control boundary cases, five source fingerprints and 43 exact control contracts pass**. The independent spiral comparison covers 27 geometry combinations.

The runner writes generated previews and `procedural-validation.json` into the ignored `build/procedural-family` directory. Timings measure GPU draw plus synchronous CPU readback on the test machine, not display refresh or physical-device performance. No app instance or hardware endpoint is used.

Reference metadata credits: Visor, Custom Wave, Spin and Plasma are catalogued under SignalRGB; Pinwheel under Ngoc Doan. Source fingerprints document the local audit and do not imply distribution permission for the original source. This batch contains independent implementations under the repository GPL-2.0-or-later license.
