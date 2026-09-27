# Continuous audio rhythm for Shaders

On Windows, Shaders can use the complete WASAPI PCM stream to detect attacks and
estimate a stable pulse. Enable audio, select the output to capture (or **System
default output (Loopback)**), then enable **Track musical pulse (continuous
audio)**. This option is off by default. Other audio effects retain their
existing 512-sample input and Shaders retains its original envelope when the
option is off.

The UI reports silence, listening, or a tempo estimate with confidence. A tempo
grid is used only after repeated evidence of periodic attacks; uncertain input
uses observed transients. Silence, unavailable capture, resets and stale data
suppress prediction. There is no free-running BPM fallback. Confidence is an
engineering consistency measure, not a calibrated probability.

This capture/DSP path runs on the existing Windows capture worker. It creates no
new daemon or analysis thread and does not save PCM. Non-Windows audio capture
remains unchanged; its rhythm getter currently returns an empty snapshot.

## Profile setting and shader interface

The native checkbox saves a Boolean in the effect's `CustomSettings`:

```json
{
  "use_audio": true,
  "rhythm_tracking": true
}
```

Normal audio endpoint settings are saved separately in `audio_settings`. The
new option does not replace existing endpoint selection, visual normalization,
color correction or device assignment. Rhythm analysis uses the continuous PCM,
independently of the legacy visualization gain or rendered frame rate.

The shader interface adds:

| Uniform | Components |
| --- | --- |
| `iRhythm` | BPM (zero when unlocked), phase, confidence, pulse envelope |
| `iOnset` | Low/mid/high positive spectral-change envelopes, transient accent |

`iAudio` and `iMusic.xy` keep their previous spectrum and energy behavior.
With tracking enabled, `iMusic.zw` become pulse-driven hue and pulse envelope.
With tracking disabled, the previous `iMusic` calculation remains in place and
the new uniforms are zero. Start resets all rhythm uniforms before rendering.

The updated `shaders/room-pulse.fs` uses this interface. Existing profiles often
embed their fragment shader, so updating the DLL does not rewrite those
programs. To upgrade such a profile, replace its embedded fragment with the
updated resource and enable `rhythm_tracking`, preserving its layout/controller
selection. Test a copy before replacing an edited profile.

## Room Pulse spectrum sensitivity

Room Pulse defaults `SCALE_SPECTRUM_BY_VOLUME` to `false`. Its `iAudio`
magnitudes already carry signal level; multiplying each bar by the
spectrum-derived `iMusic.x` attenuated quiet or narrow-band material twice.
This is separate from Windows output volume and from continuous rhythm tracking.
The VU still uses the legacy spectrum-derived envelope, rather than a raw PCM
level meter. The profile's audio gain and selected capture endpoint remain
independent settings.

The GPU regression compares both shader variants with one processed bin at
0.05 and unchanged audio gain. The former bar level is 0.002888; the corrected
level is 0.209054. The lower strip's largest color channel changes from 2 to 80
out of 255. Silence and absent input remain dark and no rhythmic pulse is
created by this correction. This verifies shader response, not the level of a
particular live audio source. Existing saved profiles embed the shader and
must also receive the changed constant.

## Evidence and limits

The capture adapter's synthetic tests cover complete packets, stereo/7.1,
44.1/48 kHz, timestamps, QPC/steady-clock agreement and reset paths. The tracker
and render envelope have separate synthetic timing tests. See
[capture tests](../tests/room-audio/CAPTURE-RHYTHM.md) and
[rhythm tests and algorithm limits](../tests/room-rhythm/README.md).

Tempo is bounded to 60–180 BPM. Metrical ambiguity, swing, legato passages,
irregular music and abrupt transitions can remain uncertain. Synthetic tests
and GPU pixels do not establish optical synchronization or perceived quality
on real music. Diagnostic logs contain state transitions, estimates and sequence
counters, never the captured PCM.
