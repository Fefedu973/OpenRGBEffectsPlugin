# Read-only live Better appearance diagnostic

This optional test links the production discovery, paired-frame source,
`AppearanceInput`, appearance preparation and GPU render graph. A minimal QRC
embeds the exact shipped BetterCapture shader files. It opens an offscreen Qt
OpenGL context, not an OpenRGB controller or an output device.

The tool reads the current Better application only when `--run` is explicit:

```text
python tests/room-better-live/run.py --qt <Qt-msvc-root> --core <OpenRGB-Room-root>
python tests/room-better-live/run.py --qt <Qt-msvc-root> --core <OpenRGB-Room-root> --run
```

Run from an x64 MSVC developer shell. Optional `--descriptor <absolute path>`
selects another Better connection descriptor. Do not pass a Bearer token.
Build output and the resulting report stay in ignored `build/room-better-live`.
The report includes saved scene IDs/names, so it should remain local.

The probe renders five current frames at 800×600, or ends after a bounded wait.
It reports aggregate readiness, raw/coverage generations, rendering schema,
pass count, output dimensions, nonblack/changed frame counts, timing and source
hashes. It does not export images, request a scene, acquire a lease, change
settings, launch capture or write to lighting devices. Rendering timestamps are
not a device FPS benchmark or evidence of optical equivalence.

## Recorded live validation

On 27 September 2026 against Effects source `e1118085dd1d793f07b6b6831b9c791dbba05cc0`
and the enabled native Better output:

- The full production input → metadata preparation → GLSL pipeline produced
  five 800×600 images using the current four-pass graph, with no OpenGL error.
- The last image contained 452,652 nonblack pixels; one image change was
  observed over the short test. No claim of continuous motion is implied.
- Total diagnostic duration was 1.694 seconds. First draw, including shader
  compilation, took 182.226 ms; median of five draws, including GPU readback,
  was 3.676 ms. These are observations for the current recipe, not all modes.
- No image was saved and no scene mutation or hardware output was performed.

The private `report.json` contains the exact C++/GLSL source hashes and current
scene catalogue. The separate synthetic appearance fixture suite remains the
test for geometric/filter comparisons; this probe verifies live interoperability.

## Optional transient scene-control test

`--run --scene-cycle` is a **separate, explicitly mutating test**. It selects the
saved scenes named `Main screen` and `Complete screen setup` through one lease,
then releases that lease to let Better restore its original scene/settings.
It requires both scenes to exist; it never creates scenes or substitutes an
arbitrary UUID. Do not use this option without authorization to change the active
scene temporarily.

The native harness snapshots the initial scene and effective settings, then
requires `ControlPhase::Effective` and an exactly matching paired image for each
target (instance, scene UUID, scene/control revisions, raw generation and minimum
sequence). Release runs on every completion/error path once a claim is created.
A manual supersession aborts further selection and is respected; the harness
never forcibly selects the original scene or reacquires after a manual edit.
The overall wait is bounded to 29 seconds plus bounded worker cleanup.

On 27 September 2026, the real current Better instance acknowledged both target
scenes and produced their matching image states. Release then produced a newer
control revision whose scene UUID and complete effective-settings object matched
the initial snapshot exactly. Total time was 3.060 seconds, with no manual edit,
image export or lighting-device output. This validates temporary scene control
and restoration, not a saved OpenRGB-profile round trip.

Full initial/target/restored metadata remains in a timestamped, ignored
`build/room-better-live/scene-cycle-*.json` file. The terminal prints only a compact
result. No lease capability or Bearer credential is saved.
