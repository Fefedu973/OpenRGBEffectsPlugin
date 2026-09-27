# ONNX Runtime C headers

These three unmodified public C headers come from Microsoft's CPU Windows x64
archive `onnxruntime-win-x64-1.30.0.zip`, released 10 September 2026. The archive
identifies upstream commit `f2c39fe2f838cf35ce7da92824f5a5e3ee6e88a7`.
Its included MIT license is preserved as `LICENSE-onnxruntime.txt`.

Official source:
https://github.com/microsoft/onnxruntime/releases/tag/v1.30.0

Archive SHA256:
`c6ba983baf5681af108599675d2a89c2d145512d02de28aed0bff177cd0ba949`

CPU `lib/onnxruntime.dll` SHA256:
`7e39e2bdbba836d98071ef28620735ba36a47c554cf794585269aecc50fab0da`

No runtime binary, PDB, archive, input data or model weights are vendored here.
The explicit build tool `tests/intelligent-ambience/inference_fetch_ort.py`
retrieves and verifies the pinned archive into ignored `build/onnxruntime`.
Packages distributing the runtime must retain its `LICENSE` and
`ThirdPartyNotices.txt`; these source-header notices are not a replacement.
