# Room scene shaders

Six original GLSL 1.10 shaders provide a small native library for the room's
Stream Deck menu: plasma, fire, aurora, radial waves, stars and bubbles. They
render through the existing `ShaderProgram` GPU path, with no browser or external
lighting service. They are also available as shader resources in the editor.

The OpenRGB Room repository's `tools/room-setup/create-extra-scenes.py` creates
local profiles from an existing spatial-rainbow profile. It retains the canonical
Full Scale map and changes only the shader preset. Output is 800 x 500 at a
requested 30 FPS; drivers keep their own device limits. Brightness and speed
remain in the standard effect settings; the named constants in shader source
control each pattern's scale or pace.

These patterns are not ports of a user's unselected SignalRGB favorites. Future
ports need an exact effect version, control values and a fixed-time visual
reference (plus the same audio input for reactive effects). The target is native
GLSL with matching controls and measurable image comparison; the website renderer
is reserved for intentionally web-based sources such as Ambilight.

The test in `tests/room-scenes/run.py` compiles each actual generated profile with
the production renderer, renders two timestamps at 800 x 500, checks visible and
changing samples, and saves a contact sheet. Run from an MSVC x64 developer shell
with `--qt`, `--openrgb-root` and `--profiles` paths. It opens no lighting devices.
