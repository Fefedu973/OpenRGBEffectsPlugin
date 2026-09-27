# Native basic effect ports

Ten original GPU assets, with metadata copied only as the public control contract:
Good Night!, Color Cycle, Neon Shift, Police Lights, Rainbow Pulse, Color Shift,
TV Static, Custom Sunrise, Crooked Waves, Quad-Color Breath. No original scripts
or media are embedded, and no JavaScript engine runs them.

`BasicEffectState.h` supplies bounded numeric state for four effects. The shared
effect class adds `State::Declarations(id)` to its GLSL prefix, merges
`State::Update(id, parameters, elapsed)` into uniforms, and calls `Reset()` when
starting a new effect. Police uses quantized steps, Color Shift retains ten
random-color layers, Quad-Color Breath retains its rotating four-color palette,
and Crooked Waves retains at most four stripe positions plus its common wrap
color. Fixed 60 Hz ticks preserve their history through control changes and do
not depend on display frame rate; catch-up is bounded to 250 ms per call.

Rainbow Pulse uses the latest spatial event as a **global** flash. Neon Shift
uses framebuffer feedback, retaining the reference's per-render-frame 5% blend.
Good Night is deliberately black and is not an OS/device power-off command.

Custom Sunrise is an explicitly approximate radial reconstruction, not a
pixel-perfect port of the finite 400-ring simulation. Integrated motion, editable
center, scale and colors work, but startup overlap, sparse recycling, and the
history of ring colors after changing Scale differ. This limitation must remain
visible in any catalogue status. Deterministic random sequences and GLSL edge
antialiasing also differ from browser rendering where applicable; each preset
records its specific boundaries.

Tests are synthetic only. `basic-family.py` checks the ten control schemas and
shader contracts. `test_basic_state.cpp` exercises the actual state helper,
including quantized speed edits, zero-speed freeze, fixed-step equivalence,
bounded histories, palette persistence and stripe-count changes. GPU tests use
the production ShaderProgram without any controller or hardware.

Validation on 27 September 2026: 3 metadata/contract groups, 41 native-state
assertions and 122 GPU checks passed with Qt 6.8.3 / MSVC at 800 × 500. The GPU
checks include 26 independent color checkpoints, control boundaries, all-black
Good Night, feedback accumulation and real production GLSL compilation. The
results are in the ignored `build/basic-family-gpu/basic-family-gpu.json`; they
do not constitute optical comparisons with SignalRGB or a device-frame-rate
measurement. `run_basic_state.py` and `run_basic_gpu.py` accept `--qt` and
`--openrgb-root` and are launched from a configured MSVC shell.
