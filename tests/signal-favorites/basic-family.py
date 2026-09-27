"""Check the ten native basic ports against their audited metadata contract."""
# SPDX-License-Identifier: GPL-2.0-or-later
import json,re,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
EXPECTED=json.loads(Path(__file__).with_suffix('.metadata.json').read_text(encoding='utf-8'))
class BasicFamily(unittest.TestCase):
    def test_complete_exact_metadata(self):
        self.assertEqual(len(EXPECTED),10)
        for slug,want in EXPECTED.items():
            with self.subTest(slug=slug):
                actual=json.loads((ROOT/'Effects/SignalFavorites/presets'/f'{slug}.json').read_text(encoding='utf-8'))
                for key in ('id','source_id','controls'):self.assertEqual(actual[key],want[key])
                self.assertTrue(actual['notes'])
    def test_native_shader_contract(self):
        for slug,want in EXPECTED.items():
            with self.subTest(slug=slug):
                shader=(ROOT/'shaders/SignalFavorites'/f'{slug}.fs').read_text(encoding='utf-8')
                self.assertIn('void mainImage(',shader)
                self.assertNotIn('#version',shader)
                self.assertNotRegex(shader,r'\buniform\b')
                defined={c['key'] for c in want['controls']}
                self.assertFalse(set(re.findall(r'\b[pt]_([A-Za-z0-9_]+)',shader))-defined)
                self.assertNotRegex(shader,r'https?://|eval\(|WebEngine|requestAnimationFrame')
    def test_honest_scope_and_feedback(self):
        specs={slug:json.loads((ROOT/'Effects/SignalFavorites/presets'/f'{slug}.json').read_text()) for slug in EXPECTED}
        self.assertTrue(specs['neon-shift']['feedback'])
        self.assertIn('not pixel-perfect',specs['custom-sunrise']['notes'])
        self.assertEqual(specs['rainbow-pulse']['tap_speed_key'],'speed')
        self.assertEqual(specs['good-night']['controls'],[])
if __name__=='__main__':unittest.main()
