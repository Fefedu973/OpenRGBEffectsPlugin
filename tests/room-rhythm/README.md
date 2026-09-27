# Continuous native rhythm analysis

`Audio/RhythmTracker.h` is an original, dependency-free C++17 DSP building block.
It consumes **every mono PCM sample**, independently of effect or display FPS.
It does not open an audio endpoint, add a bridge, infer a musical genre, or use AI.
The Windows capture adapter and shader consumer are separate integration points.

## Capture contract

```cpp
room_audio::RhythmTracker tracker; // owned only by the capture worker
tracker.Push(mono, frame_count, sample_rate, first_sample_seconds, discontinuity);
room_audio::RhythmSnapshot copy = tracker.Snapshot();
// Publish this value under the existing capture-session mutex.
```

`first_sample_seconds` is the first sample's monotonic audio timestamp, not the
time when a GUI timer happened to run. The next packet should begin at the prior
timestamp plus `frame_count / sample_rate`. Windows QPC audio packet timestamps
expressed in seconds satisfy this contract. Sample rates from 8 to 192 kHz are
accepted; a single call is limited to two seconds of PCM. Input is clamped to
[-1,1], with nonfinite samples replaced by zero. Explicit discontinuity, clock
reversal, a gap larger than two samples or 2 ms, or a sample-rate change resets
the analysis. Invalid arguments reset the state and return false.

The object performs no allocation during capture and starts no worker. Its
bounded state is under 200 kB: an at-most-8192-point FFT, eight seconds of onset
history and at most 128 onset events. `Snapshot()` itself is not synchronized;
read a published copy, never the mutable tracker, from another thread.

No-packet silence is different from a PCM packet containing zeros. A capture
adapter must mark a stale published copy unavailable, with `locked=false`,
`confidence=phase=onset_strength=0`, instead of indefinitely showing an old beat.
The current Windows policy can retain the internal tracker briefly while
publishing silence after 100 ms, then `Reset()` after 1.1 seconds without packets.
A later timestamp gap still resets rather than fabricating missing samples.
Very sparse percussion on endpoints that omit all silent packets may therefore
fall back to attacks instead of maintaining a tempo grid.

## Output and algorithm

Every 10 ms of audio, a Hann window of approximately 40–64 ms is transformed by a
radix-two FFT. Positive log-magnitude changes are measured separately over
40–250 Hz, 250–2000 Hz and 2000–12000 Hz (limited by the input's Nyquist rate).
Adaptive per-band thresholds allow mid/high-frequency attacks to work without a
kick drum. A one-hop local-peak confirmation and 50 ms refractory period produce
one onset per event across the bands.

- `audio_time` timestamps the last analyzed sample boundary.
- `last_onset_time` compensates for the analysis-window center;
  `onset_sequence` increments independently of tempo or rendered frames.
- `onset_strength` is a 120 ms attack envelope. `band_flux` contains normalized
  instantaneous positive spectral changes, not absolute band loudness.
- `sequence` counts completed analysis hops. `generation` changes on reset.
- `bpm` is the current/last estimate within 60–180. Use it as predictive timing
  **only while `locked` is true**. `phase` is 0 at the estimated beat and rises
  toward 1. It is zero when no reliable grid is available.
- `confidence` is an engineering consistency score, **not a calibrated
  probability**. Periodic correlation, sufficient observed attacks, phase
  concentration and repeated stable estimates must agree before lock.

Tempo candidates use recent onset correlation at an interval and its multiples;
weighted onset phases are checked for each candidate before choosing a grid. An
explicit supported-subdivision rule avoids automatically choosing half tempo
for alternating strong/weak beats. This is a heuristic resolution of metrical
ambiguity, not a general musical truth. Estimation runs every 250 ms. Sustained
notes cannot keep a clock locked once recent attacks disappear. Continuous PCM
silence longer than 1.1 seconds clears confidence, lock and phase; short musical
rests do not immediately erase the history.

This does not determine downbeats, meter, swing feel, song structure or semantic
accents. Half/double-time ambiguity, dense offbeats, legato music, low-level
noise, changing tempo and abrupt instrumentation remain real limitations.
When uncertain, render the actual transients and band flux; do not replace them
with a free-running metronome. Listening and annotated real-music evaluation
remain necessary before claiming improved musical perception.

## Offline tests

From an MSVC developer shell, run:

```text
python tests/room-rhythm/run.py
python tests/room-rhythm/run_envelope.py
```

The harness synthesizes all PCM itself. It never creates a WASAPI stream,
microphone capture, plugin, GPU context or hardware connection. Cases cover:

- 90/120/150 BPM at 44.1 and 48 kHz, plus 60/180 BPM boundaries;
- weak alternating beats, high-frequency offbeats and syncopation, changing
  volume, treble-only pulses and simultaneous bass/treble attacks;
- kick grids at 157/173 BPM with quiet sixteenth-note hats at 44.1 and 48 kHz;
- silence, a sustained tone, a single volume jump and irregular attacks;
- a continuous 120→150 BPM transition, pause/resume and absolute timestamps;
- arbitrary 137-versus8191-sample packets, rate boundaries, discontinuities,
  invalid arguments and nonfinite samples.

Measured regular-fixture tempos are 89.55, 120 and 150 BPM; timestamps are asserted
within 45 ms and phase within 0.065 cycles of the synthetic ground truth. These
are algorithmic fixture results, **not measured audio-to-light latency** or a
real-song quality benchmark. Identical packetization results demonstrate that
render stalls cannot discard PCM in this module, provided the adapter actually
feeds every captured sample.

The subdivision regression exposed a concrete defect in the original 90 ms
refractory interval: spectral peak timing plus 10 ms quantization could discard
legitimate hats and the following kick. At 157 BPM, the original tracker remained
unlocked throughout the final 20 seconds of a 30-second synthetic fixture. A
50 ms interval retains these real attacks (sixteenths at 180 BPM are 83 ms apart)
without weakening any confidence, correlation or phase gate. The four 157/173
BPM / 44.1/48 kHz cases now retain the intended beat grid for at least 95% of the
measured window, with BPM error below 2 and phase error below 0.15 cycles. The
complete tracker suite passes 102 assertions, including irregular-input
abstention and no duplicate simultaneous bass/treble events. This fixes that
reproduced failure; it does not establish a reliable tempo for every live song.

The separate envelope tests exercise the production `RhythmEnvelope.h` with
30/60 FPS and jittered render timing, stale/silent input, reacquisition, resets
and transient fallback. They check the absence of false acquisition pulses and
late replay of old attacks. A limited source guard confirms the optional-null
rhythm branch keeps the legacy envelope call; it is not a full renderer test.

`python tests/room-rhythm/generate_fixture.py` optionally writes an original
16-second 120 BPM mono WAV plus event metadata into gitignored
`private/room-rhythm`. It includes kick, mid-frequency accents and offbeat hats,
with a deliberately low peak below -20 dBFS. Generation never plays the file.
Any future loopback playback is a separate, explicitly coordinated integration
test; this fixture does not establish quality on real music.
