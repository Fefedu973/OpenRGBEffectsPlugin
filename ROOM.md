# OpenRGB Effects Room

Development fork for high-resolution image sources and large-device output.
Build with the matching [OpenRGB Room](https://github.com/Fefedu973/OpenRGB/tree/room-integration)
headers. The normal API5 plugin interface is retained. Generic image sinks use
the Room image interface, including remote sinks through the core's negotiated
Room SDK7 extension. Optional FrameSurface publication provides a separate
Windows shared-memory transport.

## Ambient

* DXGI Desktop Duplication replaces the usual Windows GDI capture path, with
  bounded acquisition, staging reuse and a bounded GDI fallback.
* Working canvas defaults to **800×600**, independent of LED counts. Source crop
  is prepared once; actual LED positions sample the result without one full-image
  rescale per zone.
* Advanced JSON `zone_regions` selects per-device/zone UV rectangles and rotation.
  These are low-level controls; a complete visual room editor is not included.
* Optional native image publication, channel `room-ambient`, can feed Stream Deck
  without routing every pixel through a virtual LED matrix. Keep that channel
  identical in the StreamDeckBackground `frame_surface` configuration.

See [Ambient details/tests](Effects/Ambient/ROOM-CANVAS.md) and
[DXGI details/tests](tests/room-capture/README.md).

## Shaders

* Dimensions up to 4096 and a total budget of 8,388,608 pixels (800×600 and 4K UHD
  supported by these bounds). Increasing the UI spinner alone is not the change:
  sampling avoids repeated image copies/resizes for every output zone.
* A latest-image mailbox replaces unbounded queued preview images. The GUI
  preview runs only while visible, at about 15 Hz and at most 640×360 pixels.
* Optional FrameSurface publication, channel `room-shaders`, preserves the full
  rendered image and applies brightness/temperature/tint. Only one effect may
  own a named channel at a time.
* Renderer resize and uniform updates are synchronized; audio uniforms are copied
  instead of keeping a pointer to another thread's mutable sample array.

The shader OpenGL FBO still requires a CPU readback. DXGI uses a CPU staging
readback too. These are **not** zero-copy GPU paths, and the dimension limits are
not a measured promise of 60 fps on every shader/GPU. Windows capture is SDR8;
HDR tone mapping and cursor composition are not added in this stage.

## Build and validation

Use the core fork's `tools/room-build/Build-Room.ps1` from a VS2022 x64 developer
shell, pointing `-EffectsRoot` at this directory. Initialize only
`Dependencies/QCodeEditor` and `Dependencies/SimplexNoise` if using the sibling
OpenRGB checkout; qmake receives `OPENRGB_ROOM_ROOT` explicitly.

Tests in `tests/room-ambient`, `tests/room-capture` and `tests/room-shaders` are
synthetic/offscreen and do not control the user's LEDs. A real screen-capture/GPU
throughput test and optical device validation are separate from compilation and
these regression tests. Legacy effects remain available; this branch does not
claim a universal GPU rewrite of every effect in the catalogue.
