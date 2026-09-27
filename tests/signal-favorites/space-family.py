"""Static metadata/GLSL integration checks; no GPU, application or hardware I/O.

--source-cache optionally compares local reference declarations in place. The
public fixture contains metadata and source hashes, never original source code.
Actual compilation and pixel checks belong to run.py's real OpenGL harness.
"""
import argparse
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = Path(__file__).with_suffix('.metadata.json')


class Declarations(HTMLParser):
    def __init__(self):
        super().__init__()
        self.controls = []

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag != 'meta' or 'property' not in attrs:
            return
        kind = {'combobox': 'enum'}.get(attrs['type'], attrs['type'])
        value = attrs['default']
        if kind == 'number':
            value = float(value)
        elif kind == 'boolean':
            value = value.lower() in ('true', '1')
        item = dict(key=attrs['property'], label=attrs['label'], type=kind, default=value)
        for key in ('min', 'max', 'step'):
            if key in attrs:
                item[key] = float(attrs[key])
        if kind == 'number':
            item.setdefault('step', 1)
        if kind == 'enum':
            item['options'] = attrs['values'].split(',')
        self.controls.append(item)


class Contracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixtures = json.loads(FIXTURE.read_text(encoding='utf-8'))['presets']

    def test_controls_match_reference_metadata_and_valid_defaults(self):
        self.assertEqual([len(x['controls']) for x in self.fixtures], [7, 6, 9, 8])
        self.assertEqual(len({x['source_id'] for x in self.fixtures}), 4)
        for fixture in self.fixtures:
            with self.subTest(fixture['id']):
                preset = json.loads((ROOT / 'Effects/SignalFavorites/presets' / (fixture['slug']+'.json')).read_text(encoding='utf-8'))
                for key in ('id', 'source_id', 'controls'):
                    self.assertEqual(preset[key], fixture[key])
                self.assertTrue(preset['notes'])
                self.assertEqual(len(preset['controls']), len({c['key'] for c in preset['controls']}))
                for c in preset['controls']:
                    if c['type'] == 'number':
                        self.assertLessEqual(c['min'], c['default'])
                        self.assertLessEqual(c['default'], c['max'])
                        self.assertGreater(c['step'], 0)
                    elif c['type'] == 'enum':
                        self.assertIn(c['default'], c['options'])
                    elif c['type'] == 'boolean':
                        self.assertIsInstance(c['default'], bool)
                    else:
                        self.assertEqual(c['type'], 'color')
                        self.assertRegex(c['default'], r'^#[0-9a-fA-F]{6}$')

    def test_glsl_engine_contract(self):
        for fixture in self.fixtures:
            with self.subTest(fixture['id']):
                shader = (ROOT/'shaders/SignalFavorites'/(fixture['slug']+'.fs')).read_text(encoding='utf-8')
                shader = re.sub(r'/\*.*?\*/|//[^\n]*', '', shader, flags=re.S)
                self.assertNotRegex(shader, r'(?m)^\s*(#version|uniform\s)')
                self.assertEqual(len(re.findall(r'void\s+mainImage\s*\(', shader)), 1)
                self.assertNotIn('iTime', shader, 'Motion uses integrated controls, not wall clock.')
                controls = {c['key']: c for c in fixture['controls']}
                parameters = set(re.findall(r'\b([pt]_[A-Za-z_][A-Za-z_0-9]*)\b', shader))
                for parameter in parameters:
                    self.assertIn(parameter[2:], controls)
                    if parameter.startswith('t_'):
                        self.assertEqual(controls[parameter[2:]]['type'], 'number')
                for key in controls:
                    self.assertTrue('p_'+key in parameters or 't_'+key in parameters, key)
                self.assertNotRegex(shader, r'\bwhile\s*\(')
                self.assertNotRegex(shader, r'\b(texture|texture2D|sampler2D)\b')

    def test_tap_contract_is_explicit_and_no_keyboard_claim(self):
        shader = (ROOT/'shaders/SignalFavorites/underwater.fs').read_text(encoding='utf-8')
        self.assertIn('iTap', shader)
        self.assertIn('p_tapEnable', shader)
        notes = json.loads((ROOT/'Effects/SignalFavorites/presets/underwater.json').read_text(encoding='utf-8'))['notes']
        self.assertIn('spatial keyboard events', notes)
        self.assertIn('not ported', notes)

    def test_optional_actual_source_declarations(self):
        if not SOURCE_CACHE:
            self.skipTest('Pass --source-cache to verify private reference metadata in place.')
        for fixture in self.fixtures:
            with self.subTest(fixture['id']):
                raw = (Path(SOURCE_CACHE)/fixture['source_id']/'effect.html').read_bytes()
                self.assertEqual(hashlib.sha256(raw).hexdigest(), fixture['source_sha256'])
                parser = Declarations()
                parser.feed(raw.decode('utf-8-sig'))
                self.assertEqual(parser.controls, fixture['controls'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-cache')
    args, extra = parser.parse_known_args()
    SOURCE_CACHE = args.source_cache
    unittest.main(argv=[__file__]+extra)
