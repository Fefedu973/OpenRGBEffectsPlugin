# Ambient canvas and local image output

This fork separates image resolution from the number of physical LEDs. An
800×600 image is one image, not a virtual controller with 480000 LEDs. No screen
capture, hardware control or existing profile was started by the offline tests.

## Controls

The canvas defaults to 800×600. Width and height accept 16..4096, with a maximum
of 16777216 pixels (64 MiB for one 32-bit image). This is a memory bound, not a
promise that 4096×4096 will run at 60 FPS. Start with 800×600 and a reasonable
capture rate. Crop coordinates refer to pixels of the selected captured screen.
The cropped image is scaled to the canvas, without preserving aspect ratio,
matching the previous per-zone stretch behavior. The crop is painted directly
into bounded canvas storage; a large off-screen crop cannot allocate a huge
intermediate crop image.

Screen copy samples one pixel per real LED. A linear zone spans the horizontal
middle of the canvas; a matrix covers its complete rectangular grid. Reverse
flips the horizontal sampling direction. Matrix holes and invalid LED indices
are skipped; repeated LED indices retain the final cell's sample. Sampling
plans are cached across frames and rebuilt on topology, region or reverse
changes. Smoothing keeps the existing effect-step behavior, but previous colors
are now read at the correct zone/segment offset.

Scaled average still assigns the canvas average to each zone. The average is
calculated once per new capture. Optional region rectangles affect Screen copy,
not Scaled average.

Enable **Publish full canvas to local FrameSurface** for an image consumer.
The default channel is`room-ambient`. Use a unique channel per running effect,
made of 1..64 letters, digits, hyphens or underscores. Publication works even
when no LED zones are assigned. It requires the Windows OpenRGB Room
FrameSurface header at build time and remains disabled by default.

The published frame is opaque BGRA8, sRGB, top-left origin with explicit
dimensions/stride. Effect brightness, temperature and tint are applied exactly
once. Per-device brightness and LED smoothing are not properties of the whole
canvas, so they remain on the LED path. A zero-brightness canvas is black.
The image mailbox and publisher keep the latest frame only. A static image is
republished every 500 ms so a reader with a 1–2 s freshness limit continues to show
it. This heartbeat describes the effect's liveness, not a new desktop change.
When capture temporarily fails, the last image remains until capture resumes
or the effect is stopped. Stopping the effect releases its publisher/channel;
an in-flight effect step cannot reopen it.

A busy reader causes a bounded frame drop. If the channel is already owned or
cannot be opened, a warning is logged and the LED path continues. Change the
channel or toggle publication off/on to retry. Shared memory is local to the
Windows session/user; no network port, token or LED SDK packet carries this
image.

## Optional per-zone UV projection

The profile's`CustomSettings` accepts the following optional array. Existing
profiles with no array keep full-image sampling for every zone. This is an
explicit JSON projection, not yet an automatic import of the Room layout.

```json
{
  "working_width":800,
  "working_height":600,
  "publish_frame":true,
  "frame_channel":"room-ambient",
  "zone_regions":[
    {
      "selector":{"vendor":"Example","serial":"replace-with-device-serial","zone_idx":0},
      "rect":{"x":0.1,"y":0.2,"width":0.4,"height":0.5},
      "rotation_deg":90,
      "flip_x":false,
      "flip_y":false
    }
  ]
}
```

UV coordinates are relative to the complete canvas. Rotation is clockwise in
image coordinates around the rectangle center. Samples outside the canvas are
black. The first matching selector wins. For controllers without a serial,
provide both`name` and`location`, plus`vendor` and`zone_idx`, to avoid a name-only
match. Segments additionally require`is_segment:true` and`segment_idx`.
Malformed/nonfinite rectangles, nonpositive sizes and invalid selectors are
ignored. At most 256 valid entries are retained. Coordinates are bounded to ±16,
sizes to (0,16], and rotation to ±3600 degrees.

## Offline validation

From an x64 MSVC developer prompt, with Qt 6 MSVC installed:

```bat
tests\room-ambient\run-msvc.cmd "C:\Qt\6.8.3\msvc2022_64" "C:\Projects\OpenRGB-Room"
```

The runner compiles and runs the synthetic QImage tests, then compiles the
actual Ambient.cpp and generated UI without instantiating a capturer. On
27 September 2026 with MSVC 14.44/Qt 6.8.3: 473 assertions passed. Coverage includes
size/region validation, invalid JSON, nearest sampling versus Qt on odd/even
zone sizes, matrix holes/duplicates, reverse/rotation/crop, immutable frame
ownership, zero/half brightness, channel byte order and a real shared-memory
round trip of an 800×600 frame with only 15 sampling points. The full application
build and a real monitor capture are separate validations.

The existing EffectManager still serializes effect steps and device updates
under its global lock and may step an effect again for its LED preview. Image
sequence deduplication prevents duplicate publication for that extra step, but
this patch does not claim to replace the entire engine scheduler or remove
GPU readback from shaders.

## Native image-capable controllers

Ambient and Shaders share `Effects/CanvasImage.h`, `CanvasRegions.h` and
`CanvasRouting.h`. For any whole zone advertising the optional secondary
`RGBControllerImageInterface`, they submit one immutable BGRA canvas and an
affine mapping instead of writing compatibility LEDs. The router inspects a
capability, never a device name, model, driver or transport. The same frame
allocation is shared by every output; mapping brightness carries each zone's
individual gain. Native cadence is bounded by the output descriptor, and the
lease is refreshed while the effect runs. Stop prevents an in-flight step from
submitting another native frame; the sink's lease then expires.

An unsupported zone uses the ordinary LED plan. Busy or Invalid native replies
hold the image route and never fall back to LED writes. Segments keep LED
routing until the interface has explicit subregion ownership. `flip_x` and
`flip_y` are optional booleans in each region entry; a zone's reverse flag is
combined with horizontal flip. Image-capable displays, key surfaces or future
AIO adapters can implement the same interface without another effect-specific
branch. The external FrameSurface channel remains optional.

The synthetic tests also cover two differently indexed generic sinks sharing
one frame, per-zone gain, native reply semantics, retained immutable frames,
frame-cache reuse, descriptor cadence and stop/restart behavior.

The shared Ambient/Shader reverse setting mirrors the horizontal sampling axis,
including on a 2D matrix. This preserves Ambient's prior behavior; it is not a
flat LED-index reversal. Use `flip_y` or `rotation_deg:180` for the other
orientations. A 2×2 regression compares the native affine mapping with the LED
plan. Backend matrix-map changes must emit the existing device/topology update
notification so OnControllerZonesListChanged invalidates the plan; silently
mutating cells in place without notification is not a supported cache contract.
