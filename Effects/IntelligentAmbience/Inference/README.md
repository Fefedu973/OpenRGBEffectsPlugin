# Native ONNX inference contract v1

This is an in-process CPU execution foundation for **explicit, compatible model
packages**. It does not contain a trained model. The included synthetic graphs
prove tensor execution, lifecycle and transport only; they do not perform learned
video prediction or musical understanding. A PyTorch `.pt` checkpoint, SkipBART,
M3DDM, LaMa or an arbitrary ONNX file cannot be made compatible merely by copying
it here. Its preprocessing, graph, shapes and output semantics need an explicit
export/adapter and independent validation.

## Package and tensor ABI

A package is a local directory containing `manifest.json` and `model.onnx`.
The manifest is bounded to 64 KiB; the self-contained graph to 64 MiB. Its SHA256
must match before ORT sees the graph. No external data, custom operator domain,
function, sparse tensor, graph-valued attribute or control-flow graph is allowed
in v1. `ModelGuard.h` contains the explicit operator subset. No custom-op library,
execution-provider DLL or script is loaded from the package.

Example video package (the hash must be the actual generated model hash):

```json
{
  "schema_version": 1,
  "id": "synthetic_video_test",
  "task": "video-field-v1",
  "model": "model.onnx",
  "sha256": "<64 hexadecimal characters>",
  "inputs": [
    {"name":"current_rgb", "shape":[1,3,90,160]},
    {"name":"previous_rgb", "shape":[1,3,90,160]},
    {"name":"screen_rect", "shape":[1,4]},
    {"name":"delta_seconds", "shape":[1]}
  ],
  "outputs": [
    {"name":"field_rgb", "shape":[1,3,36,64]},
    {"name":"confidence", "shape":[1,1,36,64]}
  ],
  "states": [{"input":"memory_in", "output":"memory_out", "shape":[1,64]}],
  "field_rect": [-0.5,-0.3,2.0,1.2],
  "max_age_ms": 250
}
```

All tensors are **float32 with static positive dimensions**, batch 1. Graph input
and output names/types/dimensions must exactly match the manifest plus the named
state pairs. Image and field grids are NCHW, top-left origin, positive Y downward,
at most 256 × 256. `current_rgb` and `previous_rgb` are decoded **linear RGB [0,1]**,
not sRGB bytes. Their dimensions match. `screen_rect` is `[x,y,width,height]` in
normalized canvas coordinates; `delta_seconds` is a causal elapsed interval in
`[0,0.3]`. On a discontinuity the adapter supplies a reset and consistent previous
image. The video output `field_rect` uses screen-world coordinates: screen left
is X=0, right X=1, and screen height is its height/width ratio. This field does
not contain device positions or hardware identifiers.

`audio-field-v1` instead has the sole external input `audio_pcm [1,1,N]`, declares
`sample_rate:48000`, and consumes finite mono PCM in `[-1,1]` from a causal window.
`N` ranges from 480 to 240,000 samples (10 ms through 5 seconds).
The audio output `field_rect` is in normalized canvas coordinates. Both tasks
return `field_rgb [1,3,H,W]` with linear values in `[0,1]`; optional `confidence`
has shape `[1,1,H,W]` and values `[0,1]`. An intention-vector ABI is deliberately
not invented in v1. Non-finite or out-of-range results are rejected.

There may be at most eight recurrent input/output pairs, identical fixed shapes
per pair, at most 1 MiB of state total. The worker owns and initializes these
tensors to zero. Neither the UI nor request producer supplies hidden state.
Only a valid, current, timely run commits the next state. Reset, stream epoch,
time gap over 300 ms, rejected input or failed/stale run resets memory before its
next use. Combined public inputs and outputs are each bounded to 1,048,576 floats.

## Worker and clocks

`InferenceWorker.h` is the integration interface. Constructing `Worker` creates
no thread or runtime. `Configure(absoluteDll, manifest)` starts its thread lazily,
returns a generation immediately and loads/validates the package there.
`ReadManifest()` is a separate bounded metadata-only operation for the UI.
`Config()` publishes an immutable contract only once the actual session passed
validation. `SubmitPrepared()` accepts an owning, bounded preparation callback;
it runs on the worker and must never access QWidget or wait for capture. A null
return sets `waiting-input`. There is one replaceable pending job and one run in
flight, not a growing FIFO. The C API `Run` executes on that worker with CPU only,
sequential execution, one intra-op thread and spinning explicitly disabled.

Requests contain generation, monotonically increasing epoch/sequence and a
source timestamp in `std::chrono::steady_clock` seconds (`Now()`). The adapter
must convert a capture clock to this domain. Wall time is invalid. Future,
reordered and expired inputs are rejected; changing a package or epoch invalidates
its predecessors. `Result::expires` is based on **source time**, not completion
time. The host uses `Usable(now)` before display. Metadata-only capture heartbeats
must not invent new motion observations. While a newer request is pending, a
completed older request of the same epoch may still be displayed within its lease;
otherwise a fast producer would starve every slower inference. Published results
and model contracts are immutable value-owned snapshots.

`RequestStop()` invalidates output and pending work immediately and requests
`RunOptionsSetTerminate` / `SessionOptionsSetLoadCancellationFlag`. It does not
join on the UI or render path. Generation is checked again after preparation,
loading and running. Run options remain alive while cancellation can reach them.
The destructor **joins the worker before releasing the session/runtime DLL**.
Cancellation is cooperative: no safe in-process mechanism can promise a hard
deadline for every kernel or a stuck native runtime. Preparation callbacks must
also return promptly. No thread is force-killed and no executing DLL is unloaded.

The graph preflight and tensor bounds are not a sandbox or a complete bound on
intermediate tensor memory. A mathematically valid `Expand`/`Conv` can still have
large internals. Compatible packages must be trusted and profiled before daily
use; the small fixture proves the mechanism, not universal model safety, speed
or quality. These constraints are part of the supported ABI, not hidden fallback.

## Runtime provenance

The initial supported distribution is Microsoft's **ONNX Runtime CPU 1.30.0**,
stable release of 10 September 2026, C API 30. Its official Windows x64 archive
was verified against SHA256
`c6ba983baf5681af108599675d2a89c2d145512d02de28aed0bff177cd0ba949`.
The three required C headers and MIT license are under `vendor/`; no import
library is linked. Windows opens the explicit absolute `onnxruntime.dll` using
`LoadLibraryExW` restricted to its DLL directory and standard system search paths.
The production plugin never downloads a runtime or model. Distribution must also
retain ORT's `ThirdPartyNotices.txt` beside its native binary.

Primary references checked on 27 September 2026:

- [Official 1.30.0 release and CPU archive](https://github.com/microsoft/onnxruntime/releases/tag/v1.30.0)
- [Version-pinned C API: loading, Run, cancellation and lifetime](https://github.com/microsoft/onnxruntime/blob/v1.30.0/include/onnxruntime/core/session/onnxruntime_c_api.h)
- [Official threading configuration](https://onnxruntime.ai/docs/performance/tune-performance/threading.html)
- [ONNX model construction and schema](https://onnx.ai/onnx/intro/python.html)
- [ONNX Runtime MIT license](https://github.com/microsoft/onnxruntime/blob/v1.30.0/LICENSE)

Reproducible runtime/fixture tests and their actual results are documented in
[`inference-tests.md`](../../../tests/intelligent-ambience/inference-tests.md).
