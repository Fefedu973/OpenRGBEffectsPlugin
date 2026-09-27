# Remaining SignalRGB catalogue work

Audit: 27 September 2026. Scope/status source: `signal-catalog-progress.json`; implementation count cross-checked against the 27 JSON presets. This is an inventory and a work breakdown, not a commitment that the remaining catalogue is implemented.

## Counts and units

| Unit | Count | Meaning |
| --- | ---: | --- |
| Inventory records | 509 | Catalogue, bundled and personal sources together |
| Included reference records | 324 | Scope-filtered records; duplicates retained |
| Included canonical names | 242 | Normalized names, not proof of equivalent behavior |
| Native presets in source/tested candidate | 27 | Each targets an exact reference ID |
| Installed native presets | 26 | Current published deployment metadata |
| Additional tested candidate | 1 | Pump Up Beats; not yet installed |
| Wholly unported canonical names | 215 | None of their references has a native preset |
| Unported reference records | 297 | Includes Free variants and missing sources |
| Names containing an unported reference | 216 | The 215 missing names plus Galaxies Free |
| Missing included reference sources | 10 | Algorithm/controls cannot currently be audited |

The one-preset gap is **Pump Up Beats**. Room Pulse, generic Shaders, Ambient and WebPage capabilities are not counted as ports of named catalogue effects. None of the 27 ports is certified pixel-identical to SignalRGB.

## Duplicate and variant audit

There are 82 catalogue/Free pairs among the included records. In 69 pairs the complete inline JavaScript text and recorded control metadata compare exactly. This is useful implementation reuse evidence, but it does not automatically change either source record to supported or certify external assets/optical output.

The other 13 pairs require separate treatment: twelve have different script text; Lava has the same script but different control metadata. Script differences can be small or cosmetic, so 13 is a conservative comparison workload, not a claim of thirteen different algorithms.

Aqua, Fire Visualizer, Halloween Storm, LSD Ambience, Lava, Paint Splatters, Particle Visualizer, Rainstorm, Rave Visualizer, Smoke, Sonic Bubbles, Sunset Visualizer, Vortex.

The 242 canonical names are therefore neither 324 different effects nor a sufficient proof that only 242 implementations cover every variant. A strict script-plus-control grouping yields 245 available technical groups plus 10 unavailable references; complete behavior/asset equivalence remains to be established. Galaxies Free is one of the matching pairs, yet correctly remains an unported reference record.

## Scope exclusions and boundaries

| Scope | Records | Canonical names |
| --- | ---: | ---: |
| excluded_external_api | 6 | 4 |
| excluded_game | 89 | 62 |
| excluded_personal | 14 | 14 |
| review_hidden | 74 | 71 |
| review_scope_boundary | 2 | 1 |

Browser Integration requires the browser extension; Weather Effect uses OpenWeather. Both catalogue and Free records are excluded as specialized APIs. Hidden COVID-19 Stats and CryptoFizz likewise use external statistics/market APIs and are excluded. Generic screen capture remains included.

Impossible Game is held in `review_scope_boundary`: it is a standalone interactive keyboard mini-game, rather than an external game integration. Its Free variant follows the same boundary. A game-themed animation such as Mario must not be excluded merely because of its title. Other hidden records remain unclassified; most have no reference source, so the included total is not a final total of every historical standard effect. Of the 74 hidden records still under review, 72 have no local source; Thanksgiving and Rainbow Chaos have source and need scope/variant review.

## Concrete work batches

The following primary buckets partition the 215 wholly absent canonical names. Audio and screen use actual native-source calls/controls; keyboard uses a nonempty `onCanvasTapped` handler. Audio takes precedence over keyboard (for example Tesla Coil). Rave Visualizer also has a screen-dependent layer. Aqua contains an unused `copyScreen` helper and is not counted as screen-reactive just because that function exists.

The autonomous split is preliminary: collection mutation or CPU image-buffer operations place a source in the stateful review queue; this can include a simple palette array rather than particles. These two counts are a scheduling triage, not completed per-effect algorithm audits or complexity estimates.

| Primary batch | Canonical names | First concrete ports |
| --- | ---: | --- |
| Basic/procedural, preliminary | 25 | Visor, Custom Wave, Fire and Ice, Pinwheel, Spin, Plasma, Multizone |
| Particles/stateful, preliminary | 111 | Bubbles, Rain, Fireworks, Sakura, Starlight, Falling Stars, Fireflies, Asteroid Belt |
| Keyboard | 46 | Liquid, Lightning, Heatmap, Bombing Run, Breathing Ripples, Ripples, Thermal |
| Audio | 20 | Audio Spectrum, Bars Visualizer, RGBarz, Logarithmic Visualizer, WaveScope, Sonic Bubbles |
| Screen | 3 | Screen Ambience, Average Color, LSD Ambience |
| Source recovery first | 10 | No implementation should be invented from the title |

Ports in the keyboard batch must preserve autonomous behavior and their actual tap effects. Particle/stateful batches need bounded native populations/history and reset tests. Audio batches must audit density, level and frequency scaling individually; the Pump spectrum contract is reusable but is not proof of identical calibration. Screen batches should reuse the native capture pipeline while reproducing their own controls and filtering.

## Names in each primary batch

### Basic and procedural - preliminary

Borealis; Bouncing Ball; Bouncing Logo; Cherry Berry; Custom Wave; Explosion; Fire; Fire and Ice; Fireplace; Gamer Advantage; Gradient Pinwheel; Lasers; Lollipop Rainbow; Multiverse; Multizone; Neon Fire; Neon Sunset Wave; Pastel; Pinwheel; Pixel Fill; Plasma; Spin; Vibe; Visor; Watercolor.

### Particles and stateful - preliminary

4th Dimension; 90's Effect; Amber; Aqua; Arctic; Asteroid Belt; Avatar: The Way of Water; Be Quiet!; Beach; Biohazard; Black Hole; Block Breaker; Bubblegum; Bubbles; Bursts; Calm Water; Christmas; Christmas Tree; Circuits; Color Wave; Corrosive; Cosmic Portal; Cotton Candy; Cubes; Cyber; DNA; Day and Night Cycle; Drizzle; Drones; Electric Colors; Electric Hex; Electric Space; Electricity; Embers; Ethereal Dream; Fall Leaves; Falling Stars; Fallout; Fine Arts; Fireflies; Fireworks; Fish Tank; Flags of our Users; Floating Bubbles; Flowing River; Fractals; Frost; Galaxy Ripples; Gradient Snakes; Halloween Storm; Harry Potter; Heart Monitor; Hellfire; Hot and Cold; House of the Dragon; Hyperspace; Jack O'Lantern; Jade; Jellyfish; Kaleidoscope; LSD; Lava; Lava Lamp; Lightsaber Duel; Lucky Charms; Mario; Matrix; Meteors; Nebula; Neon Grid; Neon Storm; Neurolink; New Years; Nyan Cat; Pacman; Paint Splatters; Party Parrot; Pastel Dream; Pipeline; Pixel Drops; Pixel Wave; Planetary Orbits; Poison; Pong; Popping Bubbles; Psychedelic Dream; Pulsar; Radar; Rain; Rainstorm; Resident Evil; Rick and Morty; Riptide; Ruby; Sakura; Smoke; Snake; Soda; Stack; Stained Glass; Starlight; Super Saiyan; Swirl; Synth Sun; Tetris; Tiles; Titanium; Valentine's Hearts; Voronoi; Vortex; Wormhole.

### Keyboard

Azure; Bacteria; Black Ice; Black Panther; Bombing Run; Breathing Ripples; Brimstone; Bullet Hell; Camo Fade; Candy Corn; Cobalt; Coral; Crimson; Cyber Rain; DC Universe; Dark Magic; Dark Matter; Electric; Electric Neon; Elements; Emerald Dream; Enigma; Heatmap; Ice Storm; Indigo Sky; Koi Pond; Lightning; Liquid; Magma; Marvel Universe; Neon Sunset; Night Sky; Nuclear; Nuke; Peach; Pink Lemonade; Pulses; Rainbow Data; Ripples; Stranger Things; Supernova; Thermal; Thunder Cloud; Touch Grass; Vapor Wave; Waves.

### Audio

Audio Spectrum; Bars Visualizer; Eye of Sauron; Fire Visualizer; Hydrogen; LSD Visualizer; Logarithmic Visualizer; Particle Visualizer; RGBarz; Rain Visualizer; Rave Visualizer; Ripple Visualizer; Sea Foam; Sonic Bubbles; Space Visualizer; Squares Visualizer; Sunrise Visualizer; Sunset Visualizer; Tesla Coil; WaveScope.

### Screen

Average Color; LSD Ambience; Screen Ambience.

### Source unavailable

Biomes of Minecraft; Block Party; Bloom; Chase; Christmas Lights; Lollipop; Plasma Ball; Sine Wave; Soft Ambience; Solar Flares.

## Fidelity still open on existing ports

Installation and passing GPU/UI tests do not establish a pixel-perfect conversion. In particular, Custom Sunrise replaces finite ring recycling analytically; Terminal uses original glyphs and bounded deterministic motion; Neon Nebula uses a seeded mature population; Rainbow Tap has bounded events and normalized timing; Color Cycle, Rainbow Pulse and Neon Shift have documented timing/history differences. Earlier Space/Aurora/Galaxies/Underwater and rainbow-family reconstructions also disclose randomness, population and timing differences. Pump Up Beats preserves the twenty-control design, but native FFT calibration, edge sampling and fan slices remain stated boundaries.

Only the explicitly recorded Rainbow Tap keyboard-position/duplicate-trigger observation is a physical input validation here. The remaining ports still need effect-specific optical comparisons where fidelity is required. See `signal-catalog-porting.md` and each preset's `notes`.
