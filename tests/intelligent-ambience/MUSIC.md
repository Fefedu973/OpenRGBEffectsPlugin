# MusicDirector: native observation-to-light composition

This consumes `room_audio::RhythmSnapshot` from the existing Effects capture.
It does not open WASAPI, create a capture thread, estimate tempo again, run an AI
model, or send device commands. `Push` is called once per effect render update.
The original separate prototype remains untouched.

## Clock and integration contract

Read `Shaders::CaptureSignalSnapshot()` **before** sampling `now` using
`std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count()`.
Pass both directly to `MusicDirector::Push`. Reuse that `now` for `Snapshot`,
`RenderState` and any CPU reference samples. Never subtract an effect origin or
pass GLSL `iTime`. The 150 ms freshness limit rejects old or future observations;
it intentionally does not guess an offset to make stale data look current.

The Windows implementation's `AudioPacketSeconds` converts WASAPI's QPC value
in 100 ns units to seconds. Its timestamp-error fallback uses the same steady
clock. MSVC's `__msvc_chrono.hpp` implements `steady_clock` using QPC ticks divided
by QPC frequency. Microsoft documents [GetBuffer's timestamp conversion](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getbuffer)
and [steady_clock's QPC implementation](https://learn.microsoft.com/en-us/cpp/standard-library/steady-clock-struct).
Thus these timestamps share an epoch on the supported Windows/MSVC build.
The current other-platform `AudioManager::CaptureRhythm` stub returns empty
observations; this composition stays dark rather than inventing a clock/tempo.

`Reset` on stop, source change or profile reset. `SetIntensity` accepts 0..1.
The returned structs are ordinary owned values, independent of Qt and JSON.
`RenderState` should be collected once per render, not per pixel.

| RenderState field | GLSL uniform |
| --- | --- |
| palette | iaMusicPalette (vec3, **linear** RGB) |
| controls | iaMusicControls (vec4: base light, intensity, phase, tempo enabled) |
| motion_phase | iaMusicMotionPhase (float, bounded phase) |
| accents[16] | iaMusicAccents[16] (vec4: world x/y, decayed strength, reserved) |
| bands[0..3] / bands[4..7] | iaMusicBands0 / iaMusicBands1 (vec4, reference spectrum) |
| directed rendering control | iaMusicDirected (float: 1 directed, 0 reference bars) |

`shaders/IntelligentAmbience/music.fs` is a function fragment with declarations,
no `mainImage`. It defines `vec3 iaMusicSample(vec2 world)`, matching the directed
CPU `Sample`. World x spans screen width 0..1; y uses the same width unit. It
returns linear RGB; the final compositor owns conversion to its output space.
Do not multiply its linear output by sRGB palette values. The reference branch
uses the measured eight frequency bands and `iaScreenHeight` declared by the
video fragment. CPU reference bars use the prototype's height 0.5625; the GPU
uses the configured height, with parity at that default. Direction changes do
not affect audio capture or reset the underlying tracker. Invalid/stale render
packets have intensity zero so both branches are dark.

## Behavior and limits

- Tempo/phase come from the existing tracker. Displayed BPM requires its lock,
  confidence >=0.5 and the supported 60..180 range. Confidence is the original
  heuristic score, not a probability. No meter, downbeat or emotion claim.
- Composition scenes are explicit Calm/Flow/Groove/Energetic rules, with an
  eight-second minimum dwell and two-second palette transition. Silence bypasses
  dwell and disables output immediately.
- Sixteen retained accent slots, at most four selected accents per four seconds,
  280 ms refractory. Initial observations, source generations, pauses and stale
  onsets never replay previous hits. Missed render observations affect density
  only, not the timing/location of invented intermediate accents.
- Duplicate hop observations are ignored. The capture's `PublishNoPacket`
  silent state is honored even without a new sequence number.
- Latest snapshots omit intermediate 100 Hz observations when rendering more
  slowly. This is a bounded renderer, not an audio-event queue or perfect
  downbeat/transient transcription. Smooth motion without lock is non-metrical.

Run `python tests/intelligent-ambience/run_music.py` in an MSVC x64 developer shell.
Tests use synthetic tracker snapshots, including 30/60 Hz rendering, confidence,
silence, stale/future clocks, generation/sequence changes, accent density,
intensity and concurrent readers. They do not validate musical understanding,
physical lighting, capture hardware or GPU rendering of the complete effect.

`run_music_gpu.py --qt <Qt MSVC directory> --core <OpenRGB-Room>` additionally
builds the real production `ShaderProgram`/`ShaderPass` against the fragment.
It checks directed/reference switching, default and changed aspect ratio,
accent decay, phase wrap, stale observations, silence and zero intensity. The
synthetic Windows test passed 19 checks / 13 full-frame CPU-GPU comparisons,
with at most one linear RGB8 level of difference. The pure director suite
passed 769 assertions. These test neither an actual audio endpoint nor the
complete Qt effect's lifecycle/routing.
