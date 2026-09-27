# Continuous Windows capture validation

`run_capture.py` builds two tests with an x64 MSVC developer environment:

```text
python tests/room-audio/run_capture.py
```

The existing format/lifecycle suite enumerates endpoints but opens no stream.
The new packet suite supplies synthetic interleaved PCM to the production
decoder and `AudioSession::PublishPacket`; it makes no WASAPI capture call.

On 2026-09-27, **23 existing checks and 2734 packet checks passed**. Coverage:

- 44.1/48 kHz stereo and 48 kHz 7.1, fragmented into 1–4096-frame packets;
- complete mono PCM reaches the tracker, matching a continuous mono reference;
- the original latest-512-sample window stays unchanged;
- QPC conversion and clock epoch, first-sample fallback, silent/null packets, buffer capacity;
- lost-input publication, reset, discontinuity and invalid-packet recovery;
- a rate outside the tracker's 8–192 kHz range preserves the legacy audio window
  with an empty rhythm snapshot.

The capture worker decodes a bounded packet into owned storage, releases the
WASAPI buffer, then runs DSP without the publication mutex. It publishes the
legacy window and rhythm snapshot together. Renderers copy the snapshot through
`AudioManager::CaptureRhythm`; they never access the capture-owned tracker.
Non-Windows capture behavior is unchanged and the added getter returns an empty
snapshot there.

GetBuffer's QPC position identifies the first audio frame and already uses
100-nanosecond units. A timestamp-error flag uses receipt time minus the packet
duration and marks the packet discontinuous. See Microsoft's
[IAudioCaptureClient::GetBuffer documentation](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getbuffer).

Explicit silent PCM still passes through the tracker. With no packet for 100 ms,
the published state suppresses beat prediction and clears the legacy window;
after 1.1 seconds the analysis history is reset. Endpoint reopening resets it
immediately. These tests establish data handling, not real-device latency or
tempo accuracy on arbitrary music; the tracker has a separate synthetic suite.
