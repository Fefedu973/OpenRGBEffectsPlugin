"""Metadata migration checks. Actual GLSL compilation/rendering is a separate gate.

Only metadata/defaults are fixtures; no upstream implementation or user settings.
Optional --inventory points to a private locally generated inventory and compares
the shipped controls against the original declarations, without copying sources.
"""
import argparse,json,re,unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
FIXTURE=Path(__file__).with_suffix('.metadata.json')

def normalized(meta):
    kind={'combobox':'enum'}.get(meta['type'],meta['type'])
    value=meta.get('default','')
    if kind=='number':value=float(value)
    elif kind=='boolean':value=value.lower() in ('1','true')
    c={'key':meta['property'],'label':meta['label'],'type':kind,'default':value}
    for k in ('min','max','step'):
        if k in meta:c[k]=float(meta[k])
    if kind=='number':c.setdefault('step',1)
    if kind=='enum':c['options']=meta['values'].split(',')
    return c

class MetadataTests(unittest.TestCase):
    def test_all_controls_defaults_and_identifiers_match_source_fixture(self):
        data=json.loads(FIXTURE.read_text(encoding='utf-8'))['presets']
        self.assertEqual(len(data),9)
        self.assertEqual(sum(len(f['controls']) for f in data),55)
        self.assertEqual(len({f['id'] for f in data}),9)
        for fixture in data:
            with self.subTest(fixture['id']):
                preset=json.loads((ROOT/'Effects/SignalFavorites/presets'/(fixture['slug']+'.json')).read_text(encoding='utf-8'))
                self.assertEqual(preset['id'],fixture['id'])
                self.assertEqual(preset['source_id'],fixture['source_id'])
                self.assertEqual(preset['controls'],fixture['controls'])
                keys=[c['key'] for c in preset['controls']]
                self.assertEqual(len(keys),len(set(keys)))
                for c in preset['controls']:
                    if c['type']=='number':
                        self.assertLessEqual(c['min'],c['default'])
                        self.assertLessEqual(c['default'],c['max'])
                    if c['type']=='enum':self.assertIn(c['default'],c['options'])
                    if c['type']=='color':self.assertRegex(c['default'],r'^#[0-9a-fA-F]{6}$')
                    if c['type']=='boolean':self.assertIsInstance(c['default'],bool)

    def test_shader_uniform_contract_and_all_controls_used(self):
        for fixture in json.loads(FIXTURE.read_text(encoding='utf-8'))['presets']:
            with self.subTest(fixture['id']):
                source=(ROOT/'shaders/SignalFavorites'/(fixture['slug']+'.fs')).read_text(encoding='utf-8')
                self.assertNotRegex(source,r'(?m)^\s*(#version|uniform\s)')
                self.assertIn('void mainImage(',source)
                declared={c['key']:c for c in fixture['controls']}
                uniforms=set(re.findall(r'\b([pt]_[A-Za-z_][A-Za-z_0-9]*)\b',source))
                for uniform in uniforms:
                    self.assertIn(uniform[2:],declared)
                    if uniform.startswith('t_'):self.assertEqual(declared[uniform[2:]]['type'],'number')
                for key in declared:
                    self.assertTrue('p_'+key in uniforms or 't_'+key in uniforms,key)
                # Speed belongs to the integral, never a second multiplication.
                self.assertNotIn('p_speed',uniforms)
                self.assertNotIn('iTime',source)

    def test_optional_actual_local_declarations(self):
        if not INVENTORY:self.skipTest('No private inventory supplied')
        items={e['id']:e for e in json.loads(Path(INVENTORY).read_text(encoding='utf-8'))['favorites']}
        for fixture in json.loads(FIXTURE.read_text(encoding='utf-8'))['presets']:
            with self.subTest(fixture['id']):
                source=items[fixture['source_id']]['sources'][0]
                self.assertEqual(fixture['source_sha256'],source['sha256'])
                self.assertEqual(fixture['controls'],[normalized(c) for c in source['controls']])

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--inventory')
    args,remaining=parser.parse_known_args();INVENTORY=args.inventory
    unittest.main(argv=[__file__]+remaining)
