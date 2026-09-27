# Existing capture → owned model input

`AudioManager::CapturePcmWindow(device, samples, now)` is an optional reader of an
already registered Windows AudioSession. It neither opens a device nor registers
another capture client. Call it from the model worker. Do not call it from
`StepEffect`, the GUI thread or a WASAPI callback.

The result is `room_audio::PcmWindowSnapshot` in `Audio/PcmWindow.h`:

- `Ready()` means `mono.size()==samples`, finite float mono at exactly 48,000 Hz.
- `samples` must be 1..240,000: a maximum five-second causal model input.
- `epoch` identifies continuity globally within this module lifetime; a reconnect,
  missing packet, format change, device-frame gap or timestamp discontinuity
  invalidates the previous history. `sequence` counts accepted packets.
- `source_begin` / `source_end` describe the target window in absolute QPC seconds
  (`source_end` exclusive). `captured_through` describes the latest source packet.
  `now` uses the same Windows `steady_clock` domain, not time since effect start.
- `source_rate` and `source_channels` describe the original decoded audio, before
  the existing channel average. The model receives one channel, not stereo PCM.
- `Unavailable`, `Warming`, `Stale`, `UnsupportedRate`, `InvalidRequest` and
  `Closed` contain no model samples. No zeros are invented to fill missing audio.

The ring is allocated once, lazily, on a reader. The producer appends to fixed
storage after WASAPI `ReleaseBuffer`. Its mutex protects only bounded memory
operations. Readers allocate before taking the copy lock and perform all sample
conversion after releasing it. Inference, callbacks and model execution never
hold the capture lock. This is a short-lock design, not a lock-free or hard-real-
time guarantee. It does not discard a valid packet when a reader is copying.

The maximum ring occupies 3,840,288 bytes (192 kHz × five seconds plus filter
margin). Each simultaneous reader owns at most one equally bounded source copy,
one 960,000-byte output and a shared 262,144-byte filter coefficient cache. There
is no queue of historical model windows. A held result remains safe after stop.

Native 48 kHz is copied exactly. Source rates 8–192 kHz use a 64-tap Blackman sinc
with 1024 fractional phases and a 0.94 Nyquist guard on the reader. The filter
look-back is reflected in the returned timestamps. Its table is built only when
the source rate changes. This resampler is a specified baseline, not a claim of
numerical equivalence to a future model's training frontend.

The audio callback still publishes the legacy latest-512 PCM and RhythmSnapshot.
Measured WASAPI SILENT packets are valid zero-valued PCM. No packet for 100 ms
invalidates the model window; resuming requires a complete new window. A WASAPI
TIMESTAMP_ERROR packet can still feed legacy DSP but cannot qualify an exact-time
model input. Unsupported model rates do not disable legacy capture.

## Validation

In an MSVC x64 shell, run `python tests/room-pcm-window/run.py`. All data is
synthetic. The tests compile the actual `AudioManagerWin.h` publication path and
do not open an audio stream. The existing format test enumerates endpoint names.

Coverage includes 8/22.05/44.1/48/96/192 kHz, five-second bounds, exact48k copies,
packet partition invariance, tone/alias rejection, timestamps, device counters,
generation changes, missing/invalid packets, two readers, stop/owned lifetime,
and regression of existing FFT/continuous rhythm capture.

## Learned model boundary

This supplies the declared `audio-field-v1` tensor `[1,1,N]`. It does not make
BeatNet or Skip-BART interchangeable with that interface:

- [BeatNet's handler](https://github.com/mjhydri/BeatNet/blob/master/src/BeatNet/BeatNet.py)
  resamples to 22,050 Hz, uses 64 ms analysis windows / 20 ms hops and a 272-feature
  log-spectral input. Its streaming implementation documents 84 ms frontend delay.
  A causal deployment also needs the recurrent state and particle-filter decoder.
  The current 200-bin RhythmSnapshot is not this frontend.
- [Skip-BART generation](https://github.com/RS2002/Skip-BART/blob/main/generate.py)
  first extracts 512-dimensional OpenL3 embeddings, then generates hue/value
  classes from a complete embedding sequence. The published generator is not a
  demonstrated causal PCM-to-field streaming model. Native export, frontend
  parity and causal adaptation remain separate work; no learned weights are
  included or executed here.
