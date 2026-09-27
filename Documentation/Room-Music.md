# Tri Band music canvas and detailed spectrum

This is an original shader preset using the existing Shaders effect, its native
audio input and the generic image/LED router. It does not copy SignalRGB's
proprietary effect source and does not require an external audio bridge.

`shaders/room-tri-band.fs` is embedded directly in the generated profile. A
separate renderer effect, additional C++ effect registration and a shader
resource rebuild are not required.

## Layout contract

The shader produces an 800×500 image. A 320×200 Visual Map canvas has the same
aspect ratio; positions scale continuously. Coordinates below use top-left
origin and normalized values:

| Region | X | Y | Response |
|---|---|---|---|
| Low range surfaces | 0–⅓ | 0–½ | Warm halo + low spectrum, curve and caps |
| Middle range surfaces | ⅓–⅔ | 0–½ | Cyan halo + middle spectrum, curve and caps |
| High range surfaces | ⅔–1 | 0–½ | Yellow/green halo + high spectrum, curve and caps |
| Low range strips | 0–⅓ | ½–1 | Meter growing from bottom to top |
| Middle range strips | ⅓–⅔ | ½–1 | Independent middle-range meter |
| High range strips | ⅔–1 | ½–1 | Independent high-range meter |

Place screens/keyboards/PC shapes in the upper half, vertical strip geometry in
the lower half, and single lamps at a representative point in an upper region.
Regions meet at their boundaries: keep device bounds inside the selected column
unless a device is intentionally meant to mix ranges. The low idle level is
2.5%; the shader has no full-room time-only strobe.

The upper spectrum mirrors around global Y=58 on a 320×200 layout. Its maximum
reach is Y=21–95, so a surface spanning Y=50–70 crosses the responsive center
even at low levels. The existing volume halo remains behind the individual
bars. A curve interpolates adjacent magnitudes; bright caps mark their current
filtered amplitude. They do not claim a separate temporal peak-hold detector.
The lower VU meters are unchanged and grow from Y=200 toward Y=100. Moving a
frequency within one band changes the upper spectrum without moving the lower
meter when the band's mean magnitude is unchanged.

The inputs are existing `iAudio[256]` values. The current Effects DSP calculates
64 FFT magnitudes and repeats each four times before filtering; they are not
256 independent frequency bins. This preset uses indices 0–15, 16–95 and 96–255,
sampling each group once, with separate gains. Their exact Hz interpretation
depends on the source mix sample rate and the inherited FFT processing. No BPM,
onset detector or precise crossover-frequency claim is made. The 64-bar mode
uses each real bin exactly once: 4 low, 20 middle and 40 high bins, spread across
the corresponding equal-width columns. This is a layout-oriented piecewise
frequency axis, not an evenly spaced full-width Hz axis. The 32-bar soft mode
averages adjacent pairs (2/10/20 bars), keeping every input represented.

## Detail controls

Constants at the top of `room-tri-band.fs` control the spectrum, curve, caps,
halo, idle level, noise gate and mirror position. The default is `detailed`:
64 bars with spectrum/curve/cap gains 0.62/0.22/0.18. `--detail soft` produces
32 averaged bars and gains 0.42/0.15/0.10. Both retain the same layout and VU
behavior, and generate one profile each; no extra runtime effect is required.
The existing audio controls (amplitude, smoothing, decay, filter and equalizer)
still shape the actual input before this visualization.

## Prepare an inactive profile

```text
python tools/write-music-profile.py --identity-template existing-canvas-shader-profile.json --controller-name "Music - Tri Band.json" --out music-draft.json
```

The template provides the full virtual-controller identity. The requested canvas
name is an explicit expected target, not discovery of a new live controller.
The result contains no saved physical device colors, has `AutoStart=false`, and
does not modify application configuration. Verify the new Visual Map identity
before enabling it. The profile should route only to that canvas, so its members
receive one spatial image rather than independent effects.

To upgrade only an existing music profile's embedded shader, preserving its
audio endpoint, user controls, controller identity, layout selector, FPS and
AutoStart setting:

```text
python tools/write-music-profile.py --identity-template current-music.json --shader-only --detail detailed --out upgraded-music.json
```

The command refuses to overwrite the output file or modify a multi-effect or
multi-pass template. It writes a separate JSON file and never contacts OpenRGB.
The fragment is embedded in that JSON: this asset change needs no plugin DLL
rebuild. Loading the resulting file is a separate action.

## Windows audio input

The added `System default output (Loopback)` choice records the system's rendered
output, not the microphone. Its persisted identifier is `2147483647`; it is
appended to the UI list so existing physical endpoint indices are not shifted.
The worker resolves the multimedia default output at acquisition and checks for
a change once per second. Fixed-device choices retain their existing semantics.

WASAPI interfaces are created, read and closed on their capture worker. Each
session owns a 512-sample mono history. PCM8/16/24/32 and float32/64 are decoded
using the negotiated channel count and block alignment; unsupported formats
fail closed. HRESULT failures produce a concise diagnostic and a stoppable
one-second retry. Empty/silent packets and capture gaps produce zero samples,
not uninitialized memory or a stale beat. Shared snapshots are protected by a
mutex. Shutdown wakes pending waits and joins capture workers.

The Windows capture implementation follows Microsoft's
[loopback capture contract](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording)
and [`IAudioCaptureClient::GetBuffer` packet contract](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getbuffer).
The non-Windows capture implementation is preserved. DSP input/history is now
zero initialized, and decay uses elapsed frame duration without an integer
division by zero above 60 FPS.

## Validation and limits

From an MSVC x64 developer prompt:

```text
python tests/room-audio/run.py --qt QT_ROOT --openrgb-root OPENRGB_ROOM_ROOT --profile music-draft.json
python -m unittest discover -s tests/room-audio -p test_music_profile.py -v
```

- 23 PCM/format/lifecycle assertions pass, including multichannel decoding,
  silent/null buffers, invalid formats, independent histories, stop wakeup and
  a fully zeroed missing-capture snapshot. Endpoint enumeration is read-only;
  these tests never register a valid capture client or open an audio stream.
- 29 assertions pass per detail mode using production ShaderProgram/ShaderPass and a real
  offscreen OpenGL context. Synthetic low, middle and high inputs affect only
  their assigned columns; individual first/last FFT bins move distinct upper
  bars while keeping equal-energy lower meters pixel-identical. Noise is gated,
  spatial animation works, and output is 800×500 with no GL error. PNGs and JSON
  evidence are written under `build/room-audio` (or `--output DIRECTORY`).
- Six generator tests check exact shader-only replacement, immutable input,
  audio/routing/startup preservation, safe inactive new profiles and rejected
  ambiguous templates or unknown detail modes.

Live endpoint acquisition, endpoint switching and perceived hardware timing
remain separate integration checks. The preset and tests do not claim those
checks have occurred.
