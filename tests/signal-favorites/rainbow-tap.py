"""Metadata and event-contract checks, no keyboard/audio/device capture.
The GPU fixtures are for the shared production shader harness.
"""
import json
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[2]

class RainbowTapTests(unittest.TestCase):
    def test_declared_controls_and_event_speed_binding(self):
        p=json.loads((ROOT/'Effects/SignalFavorites/presets/rainbow-tap.json').read_text())
        self.assertEqual(p['source_id'],'-Mof09C-c5QA4qMDvAGQ')
        self.assertEqual(p['tap_speed_key'],'iSpeed')
        self.assertEqual([(x['key'],x['label'],x['type'],x['default']) for x in p['controls']],
                         [('colorMode','Color Mode','enum','Rainbow'),
                          ('frontColor','Custom Tap Color','color','#ffff00'),
                          ('color','Background Color','color','#000000'),
                          ('iWaveWidth','Wave Width','number',30),
                          ('iSpeed','Wave Speed','number',30)])
        self.assertEqual(p['controls'][0]['options'],['Rainbow','Custom','Random'])
        self.assertEqual([(x['min'],x['max']) for x in p['controls'][3:]],[(5,60),(0,100)])

    def test_integrated_motion_and_bounded_events(self):
        source=(ROOT/'shaders/SignalFavorites/rainbow-tap.fs').read_text()
        self.assertNotRegex(source,r'(?m)^\s*(uniform|#version)')
        self.assertIn('iTapEvents[event]',source)
        self.assertIn('iTapMeta[event]',source)
        self.assertIn('event<64',source)
        self.assertNotRegex(source,r'\bp_iSpeed\b|\bt_iSpeed\b|\biTime\b')
        self.assertIn('tap.z>=5.0',source)
        self.assertIn('meta.y',source)
        self.assertIn('meta.x',source)

if __name__=='__main__':unittest.main()
