# Shared screen image input

`screen_source::Source` supplies immutable frames to capture-consuming effects. It does not launch Better Screen Capture, open a screen, change a scene, or access hardware. There is no WebView, extra service, HTTP control API or new process in the provider.

```cpp
#include "ScreenSources/ScreenSource.h"

screen_source::Config config;
config.channel = "better-screen-capture"; // default; editable per effect
auto input = screen_source::Source::Acquire(config);

// Render thread: short snapshot lock, no producer I/O or image copying.
auto snapshot = input->Read();
if(snapshot.Usable())
{
    const auto& frame = *snapshot.frame;
    // Upload only when generation/sequence changed; retain frame during upload.
    // frame.image is top-left BGRA (QImage::Format_ARGB32 on Windows), opaque sRGB.
}
// Release the effect's shared_ptr on stop. Other effects keep their shared reader.
input.reset();
```

Include `ScreenSources/ScreenSources.pri` once from the host qmake project and provide the OpenRGB Room root in the include path. This module contains no `Q_OBJECT` class and does not use GUI objects, QScreen or QPixmap in its worker. The host effect remains responsible for enabling the source, selecting its channel and stopping subscriptions. No shared host build files are changed by this module.

## Transport and lifetime

On Windows, the worker uses the existing `FrameSurface/FrameSurface.h` Reader and the ORGBFRM1 contract: 128-byte versioned header, bounded CPU BGRA data, stride, sequence, generation and GetTickCount64 timestamp. The producer/reader mutex commits header and pixels together; sequence alone is not a lock. The Reader validates capacity and arithmetic before allocation, with a 64 MiB wire limit. This module additionally caps width/height (default 4096 each, configurable up to 16384) and rejects nonopaque alpha. The core Reader may allocate up to the wire limit before the stricter per-effect dimension check. It never trusts unbounded header allocation sizes.

Each accepted vector becomes an owned QImage with Qt cleanup ownership. No pointer into the mapping survives `ReadLatest`; there is no additional full-frame conversion/copy when taking effect snapshots. A retained old frame remains valid after the producer publishes, closes, or restarts. Editing a copied QImage detaches its pixels. This is CPU memory sharing plus a later renderer upload, **not GPU zero-copy**.

The default poll interval is 16 ms (allowed 5–1000 ms); default TTL is 2000 ms (allowed 1–60000 ms). The core mutex wait is 5 ms. Identical complete configurations share a worker through a weak registry, limited to 64 active configurations. Releasing the last subscriber wakes and joins that worker; it does not stop another subscription. There is no accumulating frame queue. A replacement generation can restart at sequence 1, including with different dimensions within its allocated mapping capacity.

## Static frames and failures

- `Live`: a newly accepted image. `Static`: the same unexpired publication, with the same shared frame pointer.
- **Producer heartbeat:** republish an unchanged image at least every 500 ms for the default 2000 ms TTL. A publication must increment sequence, as the existing Publisher does. Updating only a private application timer or retaining a shared_ptr does not renew freshness.
- `Busy`: a brief contended read can keep the last image only until its original deadline. `Snapshot::Usable()` also checks the deadline at call time, even if the worker is delayed.
- `Stale`: no unexpired frame; no renderable frame is returned. `Unavailable`: absent, closed or dead publisher; the mapping is released so a replacement can reconnect or choose a new capacity.
- `Invalid`: malformed header, dimensions or opaque pixel data. A later valid publication recovers. `Unsupported`: non-Windows or a build without the Room transport header; no worker is launched.

The consumer chooses its stale visual policy explicitly. Do not silently switch to desktop capture, and do not reuse a stale image just because an effect still holds it. No pixel values, source URLs, keystrokes or private identifiers are logged. `Snapshot.detail` contains an aggregate diagnosis only.

Input geometry is preserved exactly. There is **no implicit crop, fit, aspect conversion, smoothing or reduction** here. The effect applies its own selected rectangle or shader transformation. A shader can use the original image for 28×20/160×100 analysis without another capture connection. The source does not identify black pixels as empty canvas; a future silhouette/metadata contract is separate.

## Better integration status

This reader is tested with the real core Publisher and isolated synthetic channels. Better Screen Capture main `ff335ac86fbf9e8f72f17c87113e2e5753a64606` did not publish ORGBFRM1 at audit time; a separate agent is implementing that producer. An isolated C# publisher prototype has already passed cross-process tests against the core Reader, but this does not claim integration in the running Better application. No scene-control API, lease command, metadata schema or MJPEG fallback is invented here. Without a publisher, the source reports Unavailable.

See `tests/room-screen-sources` for actual Qt/MSVC synthetic tests and unsupported-backend compilation. Full Effects integration and real capture performance are separate validation steps.
