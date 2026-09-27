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
