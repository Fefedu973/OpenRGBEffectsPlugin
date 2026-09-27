"""Prepare an inactive three-band audio shader profile; never contact OpenRGB."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import copy
import json
from pathlib import Path


def make_profile(template, shader, controller_name, audio_device=-1):
    result = copy.deepcopy(template)
    effects = result['plugins']['OpenRGB Effects Plugin']['Effects']
    if len(effects) != 1 or effects[0].get('EffectClassName') != 'Shaders':
        raise ValueError('Expected one existing shader profile as identity template')
    effect = effects[0]
    if len(effect['ControllerZones']) != 1:
        raise ValueError('Expected one explicit canvas identity')
    effect['ControllerZones'][0]['name'] = controller_name
    result.update(profile_name='Music - Tri Band', controllers=[])
    effect.update(CustomName='Tri Band: pulse surfaces + strip meters', AutoStart=False,
                  SelectAll=False, FPS=30, Speed=1000)
    settings = effect['CustomSettings']
    settings.update(width=800, height=500, use_audio=True, publish_frame=False,
                    frame_channel='room-music-draft', zone_regions=[], show_rendering=False)
    settings['shader_program'] = {
        'main_pass': {'type': 2, 'fragment_shader': shader, 'texture_path': ''},
        'passes': [], 'version': '110', 'width': 800, 'height': 500}
    settings['audio_settings'] = {
        'audio_device': audio_device, 'amplitude': 100, 'avg_mode': 0,
        'avg_size': 4, 'window_mode': 1, 'decay': 88, 'filter_constant': 0.55,
        'nrml_ofst': 0.04, 'nrml_scl': 0.5, 'equalizer': [1.0]*16}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--identity-template', type=Path, required=True)
    parser.add_argument('--controller-name', default='Music - Tri Band.json')
    parser.add_argument('--audio-device', type=int, default=-1)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    shader = Path(__file__).resolve().parents[1]/'shaders/room-tri-band.fs'
    profile = make_profile(json.loads(args.identity_template.read_bytes()),
                           shader.read_text(), args.controller_name, args.audio_device)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open('x', encoding='utf-8') as file:
        json.dump(profile, file, indent=2, ensure_ascii=False)
        file.write('\n')
    print('Inactive profile prepared; canvas identity and audio endpoint must be verified before use.')


if __name__ == '__main__':
    main()
