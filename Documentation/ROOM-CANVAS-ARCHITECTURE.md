# Image surfaces, LED sampling and capture: source audit and first implementation

Audit date:27 September 2026. Effects upstream inspected at
[`f90f3ec5752d0d5cc09e2bb6c206a9e009f075df`](https://gitlab.com/OpenRGBDevelopers/OpenRGBEffectsPlugin/-/commit/f90f3ec5752d0d5cc09e2bb6c206a9e009f075df),
the remote master when cloned. References below describe that baseline unless
explicitly marked as the Room implementation. The Effects checkout pins the
OpenRGB submodule to`81bbe18a84c2e507006f19dd252e397e40a56bfe`; the Room build
instead uses the sibling OpenRGB-Room headers through`OPENRGB_ROOM_ROOT`.
No capture or hardware benchmark was performed for this audit.

## Why a large canvas cannot simply be a large LED controller

An800×600 image contains 480000 pixels. That is unrelated to the 15 physical
keys of a Stream Deck or the few hundred actual LEDs in a room. One BGRA image
is 1920000 bytes; one full memory traversal at 60 FPS moves 115.2 MB/s. This is an
arithmetic lower bound, not an observed frame rate. Multiple copies, scaling,
GPU readback, USB encoding and preview repaint add costs.

The existing live preview creates a virtual matrix with a LED and map entry
for each cell: `LivePreviewController.cpp:111–142`, then reconstructs a QImage
pixel by pixel in`DeviceUpdateLEDs`, starting at149. Its UI permits 1024×1024.
Using this as the image API would create 480000 LED objects, names, colors and
map cells just to carry800×600 pixels. Large-list UI fixes can reduce the
symptoms, but they do not make this a suitable image representation.

The legacy network SDK also has 16-bit counts. In OpenRGB-Room's
`RGBController/RGBController.cpp`, `CreateColorDescription` casts colors.size()
to`unsigned short` at2403; device-description counts do the same for leds at
2454; zone matrix-map byte size is stored in`unsigned short` at3185. A 480000
pixel image cannot be faithfully encoded as one legacy LED update or matrix
description. This fork leaves those wire formats and plugin API 5 intact.

## Concrete upstream pipeline and its costs

| Area | Baseline source | Consequence |
|---|---|---|
| Effect interface |`Effects/RGBEffect.h:60–61,72–73,103–127`|An effect steps assigned ControllerZone objects and serializes settings; no shared image/canvas output interface. FPS is a per-effect value.|
| Scheduling |`EffectManager.cpp:31–55,339–444`|One thread per enabled effect, but each step takes the same global lock in addition to the zones lock. Device UpdateLEDs runs while that global lock is held. Slow synchronous output can delay other effects.|
| Preview |`EffectManager.cpp:391–400`|Preview calls StepEffect a second time. Time-based effects may advance twice and repeat image work; preview does not consume a frame already rendered for output.|
| Ambient capture mailbox |`Effects/Ambient/Ambient.cpp:77–91`|Each capture crops/copies a full image while holding the same lock used by LED processing. The contextless callback avoids explicit ownership/thread intent.|
| Ambient sampling |`Effects/Ambient/Ambient.cpp:150–245`|No zones means early return. Each linear/matrix zone creates another scaled QImage; matrix processing visits all map cells, including holes. Previous-color lookup uses controller-global indices instead of zone offsets.|
| Windows capture |`ScreenCapturer/windows/WindowsScreenCapturer.cpp:67–150`|Worker uses QScreen geometry, creates a compatible bitmap/DC path per frame, BitBlt, private Qt HBITMAP→QPixmap conversion, then QImage. No retained DXGI capture texture.|
| Shader renderer |`Effects/Shaders/ShaderRenderer.cpp:32–125`|Separate render thread and OpenGL offscreen context per renderer. Frame draw followed by an image emission. Context/surface lifetime and shared uniforms need explicit ownership.|
| Shader readback |`Effects/Shaders/ShaderProgram.cpp:50–88`;`ShaderPass.cpp:326–328`|GL computes the shader, but QOpenGLFramebufferObject::toImage returns a CPU image every frame. This is not end-to-end GPU image transport.|
| Shader consumer/preview |`Effects/Shaders/Shaders.cpp:45–52,177–249`|An image.copy and per-zone resized images are made; QPixmap conversion/queued preview update occurs even while the preview is hidden.|
| Resolution UI |`Effects/Shaders/Shaders.ui` width/height controls|Baseline cap 512 per dimension, default 128. Raising a spinbox limit alone would leave the readback, copies and per-zone work intact.|

`ControllerZone.cpp:56,165–190` already provides zone-relative`GetLED/SetLED`
and applies brightness/temperature/tint on writes. Sampling must use these
offsets and must skip`0xFFFFFFFF` matrix holes before reading old colors.

The plugin SDK in`OpenRGBEffectsPlugin.cpp:153–181` handles list/start/stop
effect requests; it does not expose a frame-upload or frame-consumer contract.
`OpenRGBEffectsPlugin.h:27–32` declares stop packet 21, while
`Documentation/SDK.md` still says 41. New image commands must not be inferred
from that stale documentation. The host's`OpenRGBPluginInterface.h:33,85,110`
declares API version 5 and virtual LED controllers, with no image-surface API.

## Implemented first boundary

```text
screen capture / shader render
             │
             ▼
  immutable latest CPU QImage  (width × height; no LED objects)
             │
             ├── cached UV sample plan ── actual LED colors ── legacy controller output
             │
             └── one adjusted BGRA image ── FrameSurface v1 ── image-capable output
```

Ambient now renders a configurable canvas, default800×600, independently of
assigned zones. The capture callback snapshots settings, paints the selected
rectangle into bounded owned image storage outside the lock, and replaces a
single immutable mailbox only if the settings revision is still current.
There is no unbounded Qt queue of captured images. Screen-copy plans store
one UV point per valid physical LED, with holes removed and duplicates resolved
once per plan build. Per-frame sampling is O(actual LEDs), not O(canvas pixels
×zones); average mode computes a single average per new image. Region mappings
are optional normalized rectangles and rotations keyed by stable device/zone
selectors. Details and limits are in
[`Ambient/ROOM-CANVAS.md`](../Effects/Ambient/ROOM-CANVAS.md).

The separate header-only`FrameSurface/FrameSurface.h` in OpenRGB-Room provides
a local Windows shared-memory transport. Its 128-byte versioned header includes
32-bit width/height/stride, pixel format, sequence, timestamp, generation and
owner identity. The image has opaque BGRA8/sRGB pixels, top-left origin, and a
64 MiB upper bound. One writer owns a safe channel name; readers consume the
latest complete frame under a bounded mutex wait. The DACL restricts access to
the current user. It does not add a network service or alter the LED SDK ABI.

Publication works without assigned LED zones. It applies effect adjustments
once to a separate image; the raw image remains available for the LED path,
whose per-zone adjustments still run through SetLED. Duplicate StepEffect
calls cannot publish the same image sequence twice. Static Ambient frames get
a 500 ms heartbeat. Effect stop releases the writer and a guarded in-flight
step cannot reopen it. The consumer decides its own crop/resolution/cadence;
for example a full800×600 frame can become a 480×272 key background without
creating480000 LEDs.

The coordinated Windows capturer changes use DXGI Desktop Duplication with
CPU staging/readback and an owned QImage pool; retained GDI DIB capture remains
a fallback. This makes the capture resource lifecycle explicit, but the
QImage API still implies GPU→CPU transfer. Coordinated Shader changes retain
GL rendering while replacing per-zone scaled images with direct sampling,
removing unconditional hidden-preview work and adding optional FrameSurface
publication. These paths require their own build/test records; Ambient's
synthetic test result does not establish screen or GPU performance.

## Generic in-process image routing

The Room implementation additionally exposes the optional secondary
`room_image::RGBControllerImageInterface` in `FrameRouting/`. It does not add
virtual methods to the existing `RGBControllerInterface` ABI. A capability
query returns a zone's preferred image dimensions and cadence; SubmitImage
accepts shared immutable BGRA pixels, an affine mapping and a bounded lease.
Accepted, Busy, Unsupported and Invalid are distinct results. Only Unsupported
permits LED fallback; an occupied image sink must not be overwritten by that
fallback. Mapping includes a per-zone brightness gain, so all outputs can
share one already effect-adjusted frame. GetImagePreview is an optional
read-only view of the current image/mapping for the core UI.

Ambient and Shaders use one common router and region parser. Native whole
zones receive images; other zones/segments receive the same generic UV LED
samples. There are no Stream Deck, vendor or model checks in these effects.
This separates producer, scene mapping and output adapter. Adding a screen,
wallpaper consumer or future image-capable controller requires an adapter,
not another hardcoded shader/capture path. Native network image transport is
a separately versioned extension being implemented by the host; the legacy
LED wire formats remain unsuitable for large images.

## Remaining engine work, with concrete boundaries

1. **Render once, distribute once.** Add a frame-producing effect contract
   alongside the LED contract. A frame sequence is advanced once; physical
   outputs and preview consume the same immutable frame. Do not call a
   stateful effect twice for preview. Preserve API 5 with a versioned optional
   extension rather than silently adding virtual methods to its ABI.
2. **Compile layout sampling.** Store per-device LED positions and an affine
   transform into a logical Room canvas. Compile normalized UV/color sampling
   plans on layout/topology changes, then sample only real LEDs. Ambient's
   JSON`zone_regions` is a bounded preparatory interface, not the finished
   FullScale editor or automatic geometry migration.
3. **Separate rendering from slow output.** The render scheduler owns immutable
   configuration snapshots and one latest frame; each output worker has one
   replaceable pending update and its own cadence. Copy the controller list
   under lock, then perform hardware I/O outside the global effect lock.
   Preserve stop/join and controller-removal lifetime guarantees.
4. **Add a GPU surface backend without pretending CPU frames are GPU frames.**
   An internal surface can carry either CPU BGRA storage or a GPU texture plus
   adapter identity, backend, dimensions, color space, generation and completion
   fence. D3D11 capture could retain a texture; LED sampling could be a compute
   gather into an N-LED buffer; image output could use GPU resampling before
   encoding. OpenGL shader textures need an explicit interoperable path or a
   deliberate readback. WGL/D3D interop cannot be assumed across adapters or
   after device loss. Shared texture handles/fences require separate lifetime
   and access rules; FrameSurface v1 is intentionally CPU-only.
5. **Measure the actual pipeline.** Record capture/render time, readback time,
   normalization/adjustment time, sampling time, publish/copy time, pending-age,
   dropped frames, output latency and preview visibility. Report frame sizes,
   devices and capture backend. A synthetic CPU loop or a Qt compile is not a
   full-GPU benchmark or a physical refresh-rate measurement.

Memory bounds remain important: one 4096×4096 BGRA buffer is 64 MiB, and capture,
normalized image, adjusted image, shared transport and reader can coexist.
The maximum accepted size is a safety limit, not the recommended default.
Remaining baseline concerns include unsynchronized general effect settings,
GUI/GL surface ownership, preview double-stepping and serialized output.
They should be fixed with targeted tests, not concealed behind a larger
resolution control.
