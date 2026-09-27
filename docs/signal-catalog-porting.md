# SignalRGB catalogue migration

The requested scope is individual GLSL implementations of the standard effects,
including screen ambience, keyboard interaction and audio visualization, with
the original controls. JavaScript interpretation and WebView rendering are not
part of this implementation strategy. Existing native ports remain available.

Game integrations and specialized external APIs such as motion detection are
excluded. Personal effects are excluded unless their title or filename contains
`free`; this exception does not re-include game integrations. The local `Free`
variants often overlap a catalogue effect, but similar names are not evidence
that their controls or algorithms are identical.

`signal-catalog-progress.json` is a metadata-only inventory, not a claim that
the catalogue has been ported. It covers cached catalogue entries, bundled
effects and eligible personal variants. Entries without a local source remain
unverified. Hidden entries require classification. Source files, embedded image
assets, account data and private filesystem locations are not redistributed.

A completed port needs:

1. Source/control inventory and clear dependency classification.
2. An original native implementation with the same controls and interactions.
3. Production GPU validation, control boundaries and relevant temporal tests.
4. Native UI persistence and canvas routing checks.
5. Installation and runtime verification; hardware observations remain separate.

Random realizations, fonts, antialiasing, feedback timing and bounded histories
can affect pixel equivalence. Each port's notes identify these limits. An
approximation must not be advertised as a pixel-identical completed conversion.

## Verified state on 27 September 2026

There are **35 distinct native presets in source and in the tested candidate**.
**26 are deployed**; nine further presets are tested but not deployed. The catalogue inventory contains 509 source records,
including excluded entries, unavailable sources and variants. It is not a count
of implemented effects; similar titles and `Free` copies do not automatically
inherit support. This work has not ported the whole catalogue.

The first deployment contained Aurora, Custom Spiral, Galaxies, Gradient,
Gradient Wave, Rainbow, Rainbow Rise, Rainbow Tunnel, Side to Side, Solid Color,
Space, Spiral Rainbow and Underwater. Deployment does not by itself establish
pixel equivalence or an optical test of every individual effect.

The second deployment added:

| Presets | Candidate status |
| --- | --- |
| Terminal, Rainbow Tap, Neon Nebula | Installed; Rainbow Tap's physical KBHE positions and lack of duplicate triggers confirmed by the user; complete optical comparisons remain pending |
| Good Night!, Color Cycle, Neon Shift, Police Lights, Rainbow Pulse | Installed after native source, GPU and UI tests; optical comparisons pending |
| Color Shift, TV Static, Custom Sunrise, Crooked Waves, Quad-Color Breath | Installed after native source, GPU and UI tests; Sunrise remains an explicitly approximate reconstruction |

The complete plugin build and production GPU checks passed for all 26 presets.
The real DLL/native UI harness passed 451 checks, including control persistence,
without starting controllers or effects. The ten basic additions also passed
41 native-state assertions and 122 GPU checks at 800 × 500, including 26 analytic
color points. Those tests exercise implementation behavior; they are not a
comparison against recorded SignalRGB output or a physical-device FPS claim.
The subsequent Pump Up Beats candidate also passes the combined build and GPU
harness, plus 474 real DLL/UI persistence checks across 27 presets. Its separate
FFT/state, capture and six-style GPU tests are documented in
[Pump Up Beats validation](../tests/signal-favorites/PUMP-UP-BEATS.md). It remains
outside the running installation until the next deliberate plugin replacement.
See [basic-family tests](../tests/signal-favorites/basic-family.md),
[UI tests](../tests/signal-favorites/UI-TESTS.md), and
[keyboard input boundaries](native-keyboard-effects.md).

The machine-readable progress file separates candidate validation, deployment,
hardware validation and fidelity notes. Each supported entry matches its actual
reference source ID, including the three explicitly identified bundled presets.
Unimplemented variants retain their own status. No placeholder effect is
registered merely to inflate the count.

## Known fidelity boundaries

The latest batch adds Visor, Custom Wave, Pinwheel, Spin and Plasma, plus Average
Color, Screen Ambience and LSD Ambience. All 35 pass the combined production GPU
suite; 604 real DLL/UI checks pass without starting capture or hardware. The new
families have separate state, metadata and GPU tests, including an independent
polyline oracle for Spin and a private temporal reference comparison for LSD.
See [procedural tests](../tests/signal-favorites/PROCEDURAL-FAMILY.md),
[screen tests](../tests/signal-favorites/screen-family.md) and
[native screen input](native-screen-input.md).

The procedural ports normalize discrete motion to a 60 Hz reference. Plasma
uses a different deterministic random field realization. Screen reduction,
Gaussian truncation and edge antialiasing have documented differences. These
limits must not be hidden behind the passing tests. Fire and Ice was deferred
because its in-place stochastic grid requires a separate stateful implementation.

- **Custom Sunrise:** analytic dense radial bands replace the finite 400-ring
  array. Startup overlaps, sparse recycling and colors retained after changing
  Scale differ. It must not be presented as a pixel-perfect completed conversion.
- **Terminal:** original hand-drawn 5 × 7 glyphs replace the browser font; bounded
  deterministic motion and trail history differ. It is autonomous, not a
  terminal emulator, and does not read typed text.
- **Neon Nebula:** persistent native framebuffer feedback is implemented, but a
  seeded mature particle population replaces the source's random births and
  startup population. Frame rate and antialiasing affect accumulation.
- **Rainbow Tap:** ring motion and lifetimes are normalized to a 60 Hz reference,
  with at most 64 live events and deterministic per-event colors. The user
  confirmed correct physical KBHE positions for A/Q/Z/W/!/*/< and no duplicate
  triggers on 27 September. This does not establish support for every keyboard.
- **Rainbow Pulse:** the white flash is global; changing its intensity while a
  flash is decaying applies immediately, unlike the source's birth-time value.
- **Neon Shift:** feedback blends five percent per rendered frame, so convergence
  depends on render FPS. Color Cycle uses a continuous hue wrap instead of the
  source's small reset discontinuity.
- The earlier Space/Aurora/Galaxies/Underwater and rainbow-family presets also
  document procedural randomness, bounded populations, trail approximations,
  direction changes or timing differences in their individual `notes` fields.

## Publication review

The 13 new native shaders were compared with their corresponding local reference
scripts: none is a verbatim script or embedded original asset. The longest exact
code-token runs were only three to six tokens (ordinary syntax). This automated
check supplements source review and is not a copyright-clearance guarantee.
The native assets, helper and tests contain no browser runtime, proprietary font
file, original image asset, account token or private absolute user path. A targeted
scan covered 90 text files in the effect, shader and test directories. The public
inventory retains names, IDs, control counts and source hashes only; local source
paths and user-selected parameter values are not included.

Further native ports will be selected and audited individually. The requested
screen, keyboard and audio effects remain in scope, with their dependencies
implemented explicitly rather than replaced by browser execution.
