# Real native ONNX tests

No screen/audio capture, device I/O, learned weights, service, Python inference
runtime or live application is involved. Python is only a build/fixture tool.
`inference_fixtures.py` emits small standard ONNX protobuf graphs from explicit
mathematical constants. Their real parser and execution engine are the native
official ONNX Runtime CPU DLL, dynamically loaded by production `Worker`.

In an MSVC x64 developer terminal, from the Effects worktree:

```bat
python tests\intelligent-ambience\inference_fetch_ort.py
python tests\intelligent-ambience\inference_run.py --qt C:\Qt\6.8.3\msvc2022_64 --core ..\OpenRGB-Room --ort build\onnxruntime\onnxruntime-win-x64-1.30.0
```

Adapt the Qt/core paths. The fetch tool retrieves only the official versioned
CPU runtime ZIP and verifies its fixed size and SHA256 before extraction. Build
outputs and synthetic packages are placed under ignored `build/` directories.
The plugin has no network download code and no ONNX import-library dependency.

Validation on 27 September 2026: **109 checks PASS**, compiled with MSVC C++17,
`/O2 /W4` against the real production worker and ORT CPU **1.30.0**. This is a count
of assertions, not 109 independent models or hardware tests. The final run also
checks the 240,000-sample causal audio limit, its overrun and fractional shapes.

Coverage includes actual video arithmetic/confidence and named recurrent state;
actual causal PCM graph execution; exact manifest/session shapes; model hash;
non-finite input/output; forbidden external weights and custom domains; truncated
protobuf; generation/epoch/sequence/time rejection; immutable older results;
preparation on the inference thread; latest-only replacement; computation that
completes after its lease; cooperative cancellation while Run is active; reload
after cancellation; stop during preparation with no late Run/publication.
An explicit reset invalidates the previous result immediately and survives
replacement of its pending request; pre-reset in-flight output cannot republish.

The fixtures deliberately do not predict scenes or infer music style. A bounded
input-dependent matrix graph is used only to make cancellation and stale output
observable. Its timing is a test mechanism, not a performance claim for a trained
model. UI integration, GPU sampling of inferred fields, package selection and
actual deployment are separate validations owned by the effect integration.

The existing production shaders were also rebuilt against the updated shared
`ShaderPass` / per-image availability inputs, without changing those shaders:
video **60 checks / 27 full-frame CPU–GPU comparisons**, maximum sRGB8 error 1;
music **19 checks / 13 full-frame comparisons**, maximum linear8 error 1.
Both use synthetic data and the checked-in `run_video_gpu.py` / `run_music_gpu.py`.
