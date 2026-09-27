"""Metadata/source boundary checks for the three distinct native screen ports."""
# SPDX-License-Identifier: GPL-2.0-or-later
import json,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
class Metadata(unittest.TestCase):
 def test_control_contract(self):
  expected={'average-color':['tapOn'],'screen-ambience':['picture_mode','blur_amount','motion_smoothing','boost','brightness','saturation','contrast'],'lsd-ambience':['waveFreq','effectSpeed','dotFade','dotMin','colorMode','cycleSpeed','color1']}
  for slug,keys in expected.items():
   spec=json.loads((ROOT/'Effects/SignalFavorites/presets'/f'{slug}.json').read_text());self.assertEqual(keys,[c['key']for c in spec['controls']]);self.assertTrue(spec['screenReactive']);self.assertEqual(len(set(keys)),len(keys));self.assertNotIn('C:\\Users',json.dumps(spec))
   for c in spec['controls']:
    if c['type']=='number':self.assertLessEqual(c['min'],c['default']);self.assertLessEqual(c['default'],c['max'])
 def test_graph(self):
  spec=json.loads((ROOT/'Effects/SignalFavorites/presets/screen-ambience.json').read_text());self.assertEqual(spec['passes'],[{'shader':'screen-ambience-color.fs'},{'shader':'screen-ambience-blur-x.fs'}]);self.assertEqual(spec['controls'][0]['options'],['Standard','Cinema','Mono','Vivid','Dominant','HD'])
  for name in ['average-color','screen-ambience','screen-ambience-color','screen-ambience-blur-x','lsd-ambience']:
   src=(ROOT/'shaders/SignalFavorites'/f'{name}.fs').read_text();self.assertIn('void mainImage',src);self.assertNotIn('#version',src);self.assertNotIn('uniform float iScreenAvailable',src);self.assertNotIn('data:image',src)
if __name__=='__main__':unittest.main()
