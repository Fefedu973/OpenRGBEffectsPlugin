"""Profile upgrades preserve routing, startup and user audio controls."""
# SPDX-License-Identifier: GPL-2.0-or-later
import copy
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('music', ROOT/'tools/write-music-profile.py')
music = importlib.util.module_from_spec(spec)
spec.loader.exec_module(music)
SHADER = (ROOT/'shaders/room-tri-band.fs').read_text()


class ProfileTest(unittest.TestCase):
    def setUp(self):
        self.template = {'profile_name': 'User music', 'controllers': [], 'plugins': {
            'Another plugin': {'keep': 42}, 'OpenRGB Visual Map Plugin': {'active_map': 'Music.json'},
            'OpenRGB Effects Plugin': {'Effects': [{
                'EffectClassName': 'Shaders', 'AutoStart': True, 'FPS': 30,
                'ControllerZones': [{'name': 'Music.json', 'serial': 'stable', 'version': 'old'}],
                'CustomSettings': {'audio_settings': {'audio_device': 2147483647, 'amplitude': 71},
                    'shader_program': {'main_pass': {'type': 2, 'fragment_shader': 'old', 'texture_path': ''},
                                       'passes': [], 'width': 800, 'height': 500, 'version': '110'}}}]}}}

    def test_upgrade_changes_only_fragment(self):
        before = copy.deepcopy(self.template)
        result = music.replace_shader(self.template, SHADER)
        self.assertEqual(self.template, before)
        effects = result['plugins']['OpenRGB Effects Plugin']['Effects']
        self.assertIn('const int SPECTRUM_BARS = 64;', effects[0]['CustomSettings']['shader_program']['main_pass']['fragment_shader'])
        effects[0]['CustomSettings']['shader_program']['main_pass']['fragment_shader'] = 'old'
        self.assertEqual(result, before)

    def test_soft_upgrade_preserves_audio_and_routes(self):
        result = music.replace_shader(self.template, SHADER, 'soft')
        effect = result['plugins']['OpenRGB Effects Plugin']['Effects'][0]
        self.assertIn('const int SPECTRUM_BARS = 32;', effect['CustomSettings']['shader_program']['main_pass']['fragment_shader'])
        self.assertEqual(effect['CustomSettings']['audio_settings'], {'audio_device': 2147483647, 'amplitude': 71})
        self.assertTrue(effect['AutoStart'])
        self.assertEqual(effect['ControllerZones'][0]['serial'], 'stable')

    def test_new_profile_stays_inactive(self):
        result = music.make_profile(self.template, SHADER, 'Other.json', 2147483647)
        effect = result['plugins']['OpenRGB Effects Plugin']['Effects'][0]
        self.assertFalse(effect['AutoStart'])
        self.assertTrue(effect['CustomSettings']['use_audio'])
        self.assertEqual(effect['ControllerZones'][0]['name'], 'Other.json')
        self.assertEqual(effect['CustomSettings']['shader_program']['width'], 800)

    def test_reject_multiple_effects(self):
        effects = self.template['plugins']['OpenRGB Effects Plugin']['Effects']
        effects.append(copy.deepcopy(effects[0]))
        with self.assertRaises(ValueError): music.replace_shader(self.template, SHADER)

    def test_room_pulse_has_consistent_map_and_name(self):
        shader = (ROOT/'shaders/room-pulse.fs').read_text()
        result = music.make_profile(self.template, shader, 'Music - Room Pulse.json', 2147483647, preset='room-pulse')
        effect = result['plugins']['OpenRGB Effects Plugin']['Effects'][0]
        self.assertEqual(result['profile_name'], 'Music - Room Pulse')
        self.assertEqual(effect['CustomName'], 'Room Pulse')
        self.assertEqual(result['plugins']['OpenRGB Visual Map Plugin']['active_map'], effect['ControllerZones'][0]['name'])
        self.assertIn('iMusic.z', effect['CustomSettings']['shader_program']['main_pass']['fragment_shader'])
        self.assertFalse(effect['AutoStart'])

    def test_reject_multipass(self):
        self.template['plugins']['OpenRGB Effects Plugin']['Effects'][0]['CustomSettings']['shader_program']['passes'] = [{}]
        with self.assertRaises(ValueError): music.replace_shader(self.template, SHADER)

    def test_reject_unknown_detail_and_changed_controls(self):
        with self.assertRaises(ValueError): music.configure_shader(SHADER, 'invalid')
        with self.assertRaises(ValueError): music.configure_shader('different shader', 'soft')


if __name__ == '__main__':
    unittest.main()
