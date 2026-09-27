# Native SignalRGB Favorites

The **SignalRGB Favorites** category contains separate native effects with Qt
controls and OpenGL fragment shaders. It does not start a browser, execute the
original JavaScript or run an external bridge. Each effect uses the existing
canvas router, so the same frame reaches ordinary LEDs, Visual Map and image
devices such as the Stream Deck.

## Coverage

The first twelve ports cover twelve of the twenty explicitly saved favorites:

| Effect | Controls | Behavior |
| --- | ---: | --- |
| Aurora | 6 | Layered moving curtains and color cycling |
| Custom Spiral | 9 | Rotating sectors with two to four selected colors |
| Gradient | 13 | Four movable color stops and arbitrary endpoints |
| Gradient Wave | 10 | Moving color gradient with color, spacing and direction controls |
| Rainbow | 2 | Continuous rainbow wave |
| Rainbow Rise | 5 | Moving radial rainbow |
| Rainbow Tunnel | 4 | Overlapping colored rings |
| Side to Side | 5 | Directional sweeps with two colors |
| Solid Color | 3 | Solid color with optional breathing |
| Space | 7 | Layered clouds and outward-moving stars |
| Spiral Rainbow | 4 | Rotating angular rainbow |
| Underwater | 9 | Moving underwater particles and preview-triggered ripples |

**Galaxies** is also included, following the explicit request for this family,
with eight controls and five selectable formations. It was installed and
configured in the reference application, but was not one of the twenty entries
marked as favorites. It is therefore an additional port, not a thirteenth
confirmed favorite. Its original GPU construction preserves the formations and
controls without reproducing the exact randomized particle trajectories.

The names, property keys, labels, ranges, options and default values correspond
to the inspected source metadata. Private preferences are imported separately;
they are not shipped as defaults. Implementation and shader source are original.

These are not blanket pixel-identical claims. Deterministic procedural particles
replace JavaScript randomness; antialiasing and some accumulated frame history
differ. In particular, Rainbow Tunnel does not retain every previous frame.
Each preset's `notes` describes its specific differences. Underwater's Tap Effects
responds to clicks in the native preview; spatial global keyboard input is not
implemented by this category.

Terminal, Pump Up Beats, Neon Nebula and Rainbow Tap are not included in these
twelve ports. The remaining four favorite entries are screen-capture variants
and a local ambient demonstration. Existing Room Pulse and Web Page are separate
effects, not renamed claims of faithful ports of these favorites.

## Controls, rendering and persistence

Controls are uniforms, not strings interpolated into shader code. Numeric values
are finite and bounded, enum options validated, and colors normalized by Qt.
Every numeric control also has an integrated clock (`t_<key>`), so changing speed
does not reset the animation and zero freezes the associated phase. Stop/resume
does not advance the phase by the time spent stopped. Particle seeds are stable.

The default canvas is 800 × 500 at 60 FPS; resolution is configurable. A single
bounded preview consumes the latest frame and does no image conversion while
hidden. The hardware routing retains the latest-frame behavior of the room fork.
No per-pixel device configuration is hardcoded into these effects.

Profiles save the effect identity and controls, not duplicated shader programs.
Shipped resources remain the canonical implementation after upgrades. Standard
brightness, color correction, controller selection and canvas regions continue
to apply. Resolution, preview state and native controls survive profile reloads.

## Room profile preparation

`tools/create-favorite-profiles.py` creates profiles named `Favori - <name>` from
an existing profile containing one canvas effect. It preserves the controller
selection and Visual Map reference and never changes the source layout.

```text
python tools/create-favorite-profiles.py --base "Full Scale - Rainbow.json" --out prepared
```

An optional `--preferences <private-qt-export.json>` imports a read-only export of
the original settings. Missing or invalid values use the declared default. The
output directory must be new, preventing accidental overwrites of edited presets.

## Validation

- `tests/signal-favorites/rainbow-family.py`: metadata and shader contract checks.
- `tests/signal-favorites/run.py`: real production Qt/OpenGL compilation, 800 × 500
  rendering, color-control boundary cases and seventeen analytic color checkpoints.
  It saves a contact sheet and JSON results under `build/signal-favorites`.
- `tests/signal-favorites/run_ui.py`: the built Effects DLL loaded with a fake
  API and no hardware; native widgets, normalization, save/reload and ordinary
  Shaders compatibility.

GPU test timings include synchronous framebuffer readback. They are not measured
LED refresh rates or evidence of optical equivalence to every original effect.
