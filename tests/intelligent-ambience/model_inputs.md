# Native model adapters

`model_inputs_run.py --qt <Qt MSVC x64> --jom <jom.exe> --core <OpenRGB-Room>`
builds the production `ModelInputs.cpp` with QtCore/QtGui and MSVC `/W4 /WX`.
Validation on 27 September 2026: **154 checks pass**. No device, capture,
personal content, learned weights or inference runtime is opened by this test.

The video adapter accepts the fixed `video-field-v1` contract: current/previous
linear RGB NCHW (up to256×256), delta seconds and explicit normalized screen
rectangle. sRGB bytes are decoded **before** area reduction/bilinear enlargement;
straight alpha is composed over black in linear light. First frame, source/model
generation, dimensions/calibration changes and gaps over250ms reset history.
A full-frame mean absolute linear RGB difference above0.35 also resets it: this
is a conservative cut heuristic, not semantic scene recognition. Reset means
previous=current, delta=0. Duplicate/late sequences are omitted.

The audio adapter consumes an already captured `PcmWindowSnapshot`; it never
starts capture. `audio_pcm` is the complete48kHz mono window (480–240000samples).
The request uses its exclusive `source_end` and its session/reset epoch, including
the explicit resampler look-back. Missing, malformed, future or stale windows
are rejected. Tests include the production ring/resampler on synthetic44.1kHz
and48kHz constant packets with eight-channel source metadata.

`FieldImage` converts planar linear RGB to immutable, top-left RGBA32F. Optional
confidence becomes alpha, without premultiplying RGB. Finite excursions are
clamped; nonfinite values, wrong shapes, missing outputs and expired results
reject the whole image. Expiry never extends beyond the model's source-time
lease. Field placement remains the manifest's `field_rect`, applied by the
effect shader rather than by this adapter.

The asynchronous completion regression supplies an explicit Step clock older
than completion, then a refreshed read clock: only the latter admits the result,
without changing its source timestamp or expiry. No timing-dependent sleep is
used. The audio tests already cover session/epoch changes and recurrence reset.

Instances belong to the inference worker. The UI/render thread only passes
owned snapshots into `SubmitPrepared`; it must not invoke PCM resampling or
image preparation synchronously. These tests prove tensor/temporal contracts,
not predictive quality or end-to-end hardware latency.

## Complete DLL / ONNX / GPU path

`run_ui.py --run-only --qt <Qt> --jom <jom.exe> --core <OpenRGB-Room>
--dll <Effects.dll> --ort <onnxruntime.dll>` additionally creates two small local
ONNX protobuf fixtures using `model_inputs_ui_fixtures.py`. They consume the
declared inputs and output a known red video field / green audio field. They
contain only mathematical constants, no learned weights. The effect's visible
synthetic-source mode provides its moving video and48kHz440Hz PCM; no personal
source, network service, audio device or controller is opened.

27 September2026: **790 checks passed** using the real complete Effects DLL,
ORT1.30.0CPU and production GPU renderer. This includes all35 previous presets,
model/profile fields and malformed settings; visible red outside the calibrated
screen; visible green from the audio model; disable/re-enable/reload;
stop/restart; and invalid-manifest error with procedural fallback. Polling/UI
field checks contribute to the assertion count; these are not790 independent
scenarios. A missing runtime, recurrent state and invalid model graph are covered
separately by `inference_tests.cpp`, not claimed as tested by this UI harness.

Tested DLL SHA256:
`83DF29983C6BB9596A4269CFCB24257495EAE19B2257A1CF43A2244FFB56CC00`.
ORT DLL SHA256:
`7E39E2BDBBA836D98071EF28620735BA36A47C554CF794585269AECC50FAB0DA`.
Local log: `build/intelligent-ambience-ui/model-ui-run.log`.

This validates plumbing and lifecycle, not learned-model quality, real media
performance, the room's screen calibration or physical device output.
