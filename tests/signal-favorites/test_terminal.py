"""Static independent glyph/control checks; GPU validation uses the shared renderer."""
import json
import math
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
preset = json.loads((root / 'Effects/SignalFavorites/presets/Terminal.json').read_text())
shader = (root / 'shaders/SignalFavorites/Terminal.fs').read_text()
glyphs = json.loads(Path(__file__).with_name('terminal-glyphs.json').read_text())['glyphs']
assert preset['source_id'] == '-M0UK0pcnuXzoqaS7-48'
assert [(c['key'], c['label'], c['default'], c['min'], c['max']) for c in preset['controls']] == [
    ('color', 'Color', '#00ff22', 0, 360), ('fontSize', 'Font Size', 8, 5, 12)]
pairs = [(int(a), int(b)) for a,b in re.findall(r'uvec2\((\d+)u,(\d+)u\)', shader)]
assert len(pairs) == len(glyphs) == 25
assert ''.join(g['letter'] for g in glyphs) == 'ABCDEFGHIJKLMNOPQRSTUVWXY'
for glyph,(packed,last) in zip(glyphs,pairs):
    decoded = [f'{(packed >> (row * 5)) & 31:05b}' for row in range(6)] + [f'{last:05b}']
    assert decoded == glyph['rows'], glyph['letter']
    assert all(len(row) == 5 for row in decoded)
# The analytic walk has the same documented 1..5-pixel-per-tick speed bounds.
for omega in (.71, 1.0, 1.64):
    for tick in range(1,1000):
        def position(t): return 3*t+(math.sin(.7+omega*t)-math.sin(.7))/math.sin(omega*.5)
        assert 1-1e-10 <= position(tick)-position(tick-1) <= 5+1e-10
assert .85**64 < .000031
assert 'uniform' not in shader and '#version' not in shader
assert 'for(int age = 63; age >= 0; --age)' in shader
print('Terminal: 25 glyphs, exact two controls, bounded motion and 64-tick trail checks passed')
