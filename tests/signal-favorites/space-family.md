# Space-family native reconstructions

Four original GLSL implementations retain the declared metadata of Space,
Aurora, Underwater and Galaxies (30 controls total). The JSON presets use the
reference defaults, not private saved user preferences. Source identifiers and
SHA-256 hashes in the metadata fixture identify the locally inspected versions.
No reference implementation, textures or private settings are redistributed.

Run `python tests/signal-favorites/space-family.py` for metadata and shader
integration checks. An optional `--source-cache <local SignalRGB effects cache>`
compares the fixture with the locally installed HTML declarations in place.
These static checks do not compile GLSL or establish visual fidelity. The common
`run.py` OpenGL harness covers real compilation and rendered pixels separately.

## Semantics and limits

| Preset | Preserved composition | Deliberate approximation |
|---|---|---|
| Space | Four cloud layers, outward stars, independent color cycling | Stable random seeds, analytic star lifetimes and normalized cloud phases |
| Aurora | Drifting vertical curtains, five-stop gradients, density and trails | Smooth wandering and analytic trails replace random walks and framebuffer history |
| Underwater | Rotatable shimmers, directional movement, water gradient and rings | Bounded spatial population/trails; automatic ring density approximates the spawn scheduler |
| Galaxies | Five formations, independent stars, local edge compression and global transform | Analytic gradients, stable orbital clouds and capped expanding-arm populations |

All phase-based movement uses the engine's integrated numeric controls. Setting
the animation speed to zero freezes motion; changing size or color remains
effective. This differs from reference effects that skip drawing an entire
Canvas frame at zero speed. Enabling color cycling uses the existing integrated
phase rather than resetting it to zero on every toggle.

Underwater supports the latest native preview click through `iTap=(x,y,age,active)`
in 320 by 200 design coordinates, with y pointing down. Spatial keyboard events,
simultaneous tap histories and exact persistent Canvas blending are not ported.

Galaxies X/Y controls set the scale/rotation pivot, preserving the source's
otherwise surprising behavior: at neutral scale and rotation they do not move
the galaxy. Edge compression is local to the galaxy, while the star field is
unaffected. A zero galaxy scale or edge factor hides its zero-area geometry.

No pixel-identical or identical random-history claim is made. Before deployment,
inspect real renders at default settings, numeric bounds, each Galaxy Type,
color cycling, frozen speed and an enabled/disabled Underwater preview tap.
