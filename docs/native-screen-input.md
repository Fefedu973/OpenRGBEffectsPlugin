# Native screen input and Better appearance

Screen Ambience, Average Color and LSD Ambience share a native image source and
existing canvas/device routing. These are the **three screen-enabled ports**;
adding a source selector does not port every upstream effect. The integration
uses native workers and GLSL, without a WebView, JavaScript runtime, virtual LED
matrix or additional bridge process. Better owns its capture sources and scenes.

## Select a source

| Screen source | Behavior |
| --- | --- |
| Native display | Captures the selected desktop display while the effect runs. |
| BetterScreenCapture — automatic connection | Reads the connection descriptor, discovers actual raw/coverage channels and lists saved scenes. |
| Shared memory channel (advanced) | Reads an explicitly named generic ORGBFRM1 publisher, without Better discovery, scenes or appearance metadata. |

The default descriptor is
`%LOCALAPPDATA%/Better_SignalRGB_Screen_Capture/ApplicationData/NativeOutput/connection.json`.
Leave **Connection file** empty for that location, or supply an explicit absolute
path for another profile/test installation. Automatic mode discovers the current
instance's channels; it does not assume a channel named `better-screen-capture`.
The editable channel field belongs only to advanced shared-memory mode.

Profiles save `screen_source.kind`, display identity, optional connection-file
path, scene UUID and appearance choice. They never save a Bearer token or lease
capability. Screen Ambience can passively follow Better with:

```json
"screen_source": {
  "kind": "better",
  "connection_file": "",
  "scene": "",
  "follow_better_appearance": true
}
```

Discovery runs only while the effect runs or its source panel is visible. Image
capture/subscription follows effect lifetime. This consumer never launches
Better or silently falls back to another screen. Missing, invalid or expired
input produces black and an explicit source status.

## Authentication and temporary scenes

The descriptor supplies an exact IPv4 loopback HTTP endpoint, instance identity
and Bearer credential. The worker validates these before using the documented
discovery, scenes, status, generation-state and lease endpoints. It bypasses
proxies, refuses redirects, does not persist cookies, and bounds response sizes
and deadlines. Public snapshots/logs contain no credentials, URLs or remote
error bodies. There is no manual token field in the effect profile.

**Follow active Better scene** is the default and is passive: it acquires no
lease. Selecting a saved scene instead requests temporary ownership while that
effect runs. This changes **one global Better scene**, not a private capture
canvas. Effects requesting the same scene share ownership; incompatible requests
report a conflict. Stopping the last owner releases the lease so Better can
restore its previous state. Appearance preferences are never modified.

An HTTP 202 is pending, not proof that a frame contains the requested scene.
Rendering requires the matching instance, scene UUID and **exact** control/scene
revisions in bound metadata. Higher revisions can mean a manual override and
are not accepted as the old request's success. Supersession or a conflicting or
uncertain lease blocks automatic reacquisition for that intent. Releasing or
changing the selection can issue a new request.

Leases last 30 seconds and normally renew after ten seconds once the frame
acknowledgement is effective. Renewal also changes the control revision; it is
not sent during an outstanding acknowledgement. Pending/conflicting input is
black rather than a stale scene. Renewal does not itself reset screen-effect
history. Final release uses bounded, best-effort HTTP cleanup; if acquisition
never returned a capability, server expiry is the fallback. See
[discovery/control tests](../tests/room-better-discovery/README.md) for the API and
cleanup limits.

## Raw effects and following Better appearance

Average Color and LSD Ambience consume Better's **raw composite** and apply
their artistic processing. Screen Ambience does likewise when **Follow Better
appearance** is off. Raw pixels omit Better's global placement, filters and
halo; reading them does not require disabling Better's persisted glow. Existing
artistic controls and histories remain available in raw mode.

On Screen Ambience, **Follow Better appearance (native placement, filters and
glow)** defaults to enabled when using Better mode. It replaces that artistic
processing with the published native rendering recipe. The resulting canvas
follows Better's version-one settings:

- Screen placement/dimensions and smooth/pixelated interpolation.
- Hue, brightness, saturation, optional picture blur, and Standard, Cinema,
  Mono, Vivid, Dominant or HD picture modes.
- Halo enabled/fullscreen, source visibility and Classic/Soft/Contours styles.
- Halo blur, spread, saturation, intensity, cutoff, edge depth, edge mix,
  reach and fade.

Settings come from immutable published metadata; they are neither duplicated as
global preferences nor written back to Better. Source crop, opacity, mirrors,
rotation and composition are already represented by the raw image and source
geometry and must not be applied twice. OpenRGB still controls output size and
device/layout routing. Following the recipe does not imply browser-identical
edge rasterization; see the measured Contours limitation below.

## Frame identity, coverage and freshness

Windows images use the core's ORGBFRM1 contract: a versioned 128-byte header,
opaque top-left BGRA8 sRGB, stride, sequence, generation, timestamp, named mutex
and publisher lifetime. Screen inputs accept at most 4096 pixels on either axis
and 64 MiB per transported frame. Reads validate dimensions, arithmetic, stride,
alpha and freshness before exposing pixels.

Appearance requires **raw + generation metadata + exact coverage**, rather than
whichever status is current. The paired worker caches at most eight immutable
metadata generations per instance. A state's raw sequence marks the first frame
of that generation, so newer raw sequences in the same generation are valid;
coverage must match the generation and sequence declared by the envelope. A
producer restart, inconsistent metadata, missing coverage or stale input cannot
be combined with an older image. Black pixels are still covered pixels: neither
RGB black nor raw alpha substitutes for the grayscale opacity mask. See the
[pairing policy and worker tests](../tests/room-better-control/README.md).

Better refreshes the shared header timestamp about every 250ms for static
images, without changing sequence or copying pixels. The reader publishes a
fresh immutable wrapper sharing the same image and derives its expiry from the
actual header age. This heartbeat requires **no additional pixel upload**.
Holding an old snapshot, polling or advancing a local timer never renews
freshness. The default TTL is 2000ms and is checked again at draw time. See the
[low-level reader contract](../ScreenSources/README.md).

## Native rendering and bounds

Discovery, authenticated requests, mapping reads and appearance preparation run
outside GUI/rendering steps. Consumers read immutable snapshots through short
locks. Capture pixels become owned CPU images and are uploaded on the renderer
thread when pixel identity changes; this is not GPU zero-copy.

The appearance graph executes on that same renderer thread and GL context. It
allows at most 32 acyclic passes and 128MiB of RGBA16F intermediate render targets.
Raw pixels, coverage and a numeric geometry atlas are inputs. Placement, tone,
dilation, blur and composition execute in native GLSL; intermediate color images
stay on the GPU. The final image is read back once for existing canvas/device
routing. CPU preparation of source geometry is cached by rendering recipe and
output dimensions, rather than rasterizing every color frame. Input storage,
the geometry atlas and final image are additional to the intermediate budget.

Invalid schemas/graphs, excessive sizes, compilation failures and expired input
produce black with a source/renderer diagnostic. There is no unbounded frame
queue. Effects sharing input retain separate artistic state and relinquish their
own subscriptions and scene ownership when stopped.

## Verification and release status

As of the 2026-09-27 candidate, **35 native presets are in source/tested and 26
are installed**. The three screen ports are among the nine additions awaiting
deployment. The Better integration is a further candidate change: controlled
tests do not establish installation or connection to the user's running Better
application. The [catalogue progress file](signal-catalog-progress.json) records
source, candidate and deployment separately.

The producer-side native contract is included in Better 1.4 (source commit
`6979d38`). This candidate has not yet been validated against a running Better
instance with native output enabled; absence of its descriptor is reported as
unavailable rather than treated as a working connection.

Validation is split by layer:

- [Discovery/control](../tests/room-better-discovery/README.md): 167 checks using
  temporary descriptors and synthetic authenticated HTTP, including delayed
  acknowledgements, renewals, conflicts, cancellation and cleanup.
- [Frame pairing](../tests/room-better-control/run_frames.py): 48 checks using
  real ORGBFRM1 publishers and the HTTP client, including missing generations,
  invalid coverage, instance replacement and expiry. No personal service used.
- [Shared reader](../tests/room-screen-sources): actual Writer/Reader,
  timestamp-only heartbeat, ownership, contention, producer death/reconnection
  and unavailable-backend compilation.
- [Dynamic textures](../tests/room-shaders/run_dynamic.py) and
  [GPU graph](../tests/room-shaders/run_graph.py): production OpenGL upload,
  orientation, no-upload heartbeat, expiry and bounded multipass execution.
- [Native appearance fixtures](../tests/room-better-appearance/run.py): actual
  preparation/GLSL compared with Better's synthetic rendering fixtures. Pixel
  differences are reported separately from structural/GPU assertions; a
  successful run alone is not an optical-equivalence gate.
- [Screen ports](../tests/signal-favorites/screen-family.md): numeric state,
  original controls and production GLSL. Combined DLL/UI suites check preset
  registration and source-setting persistence without hardware capture; the
  current 35-preset candidate passes 623 real DLL/UI assertions.

The current appearance report contains 575 structural/GPU/optical assertions. Three
geometry/raw fixtures match exactly; filters without halo have mean absolute
error at most 0.195 on 8-bit channels, Classic 0.166 and Soft 0.359 in the tested
fixtures. After matching repeated boundary coverage, Contours fullscreen with
hidden sources has mean error 0.878/255 (0.779 against the WebGL reference), and
the high-quality crop case 0.394. **Localized antialiasing differences remain**:
the fullscreen fixture has seven occupancy differences among 64000 pixels and
isolated channel errors up to 170. These are synthetic-fixture results, not
coverage of every setting or physical-device performance. No pixel-perfect or
real-application integration claim is made.
