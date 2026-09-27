"""Prepare an inactive three-band audio shader profile; never contact OpenRGB."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import copy
import json
from pathlib import Path


def configure_shader(shader, detail='detailed'):
    if detail not in ('detailed', 'soft'):
        raise ValueError('detail must be detailed or soft')
    if detail == 'soft':
        for old, new in (
            ('const int SPECTRUM_BARS = 64;', 'const int SPECTRUM_BARS = 32;'),
            ('const float SPECTRUM_GAIN = 0.62;', 'const float SPECTRUM_GAIN = 0.42;'),
            ('const float CURVE_GAIN = 0.22;', 'const float CURVE_GAIN = 0.15;'),
            ('const float CAP_GAIN = 0.18;', 'const float CAP_GAIN = 0.10;')):
            if shader.count(old) != 1:
                raise ValueError('Shader control block changed; update soft preset')
            shader = shader.replace(old, new)
    return shader


def replace_shader(template, shader, detail='detailed'):
    """Change only the embedded fragment, preserving routes and user settings."""
    result = copy.deepcopy(template)
    effects = result['plugins']['OpenRGB Effects Plugin']['Effects']
    if len(effects) != 1 or effects[0].get('EffectClassName') != 'Shaders':
        raise ValueError('Expected one existing shader profile as identity template')
    program = effects[0]['CustomSettings']['shader_program']
    if program['main_pass']['type'] != 2 or program.get('passes'):
        raise ValueError('Expected a single buffer shader pass')
    program['main_pass']['fragment_shader'] = configure_shader(shader, detail)
    return result


def make_profile(template, shader, controller_name, audio_device=-1, detail='detailed', preset='tri-band'):
    result = replace_shader(template, shader, detail)
    effects = result['plugins']['OpenRGB Effects Plugin']['Effects']
    effect = effects[0]
    if len(effect['ControllerZones']) != 1:
        raise ValueError('Expected one explicit canvas identity')
    effect['ControllerZones'][0]['name'] = controller_name
    if preset not in ('tri-band', 'room-pulse'):
        raise ValueError('Unknown music preset')
    profile_name = 'Music - Room Pulse' if preset == 'room-pulse' else 'Music - Tri Band'
    result.update(profile_name=profile_name, controllers=[])
    if 'OpenRGB Visual Map Plugin' in result['plugins']:
        result['plugins']['OpenRGB Visual Map Plugin']['active_map'] = controller_name
    effect.update(CustomName='Room Pulse' if preset == 'room-pulse' else 'Tri Band: spectrum, pulse surfaces + strip meters', AutoStart=False,
                  SelectAll=False, FPS=30, Speed=1000)
    settings = effect['CustomSettings']
    settings.update(width=800, height=500, use_audio=True, publish_frame=False,
                    frame_channel='room-music-draft', zone_regions=[], show_rendering=False)
    settings['shader_program'] = {
        'main_pass': {'type': 2, 'fragment_shader': configure_shader(shader, detail), 'texture_path': ''},
        'passes': [], 'version': '110', 'width': 800, 'height': 500}
    settings['audio_settings'] = {
        'audio_device': audio_device, 'amplitude': 100, 'avg_mode': 0,
        'avg_size': 4, 'window_mode': 1, 'decay': 88, 'filter_constant': 0.55,
        'nrml_ofst': 0.04, 'nrml_scl': 0.5, 'equalizer': [1.0]*16}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--identity-template', type=Path, required=True)
    parser.add_argument('--controller-name')
    parser.add_argument('--preset', choices=('tri-band', 'room-pulse'), default='tri-band')
    parser.add_argument('--audio-device', type=int, default=-1)
    parser.add_argument('--detail', choices=('detailed', 'soft'), default='detailed')
    parser.add_argument('--shader-only', action='store_true',
                        help='Preserve every profile setting except the embedded fragment')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    if args.preset == 'room-pulse' and args.detail != 'detailed':
        parser.error('Room Pulse uses its own control block; --detail soft applies to tri-band only')
    shader = Path(__file__).resolve().parents[1]/('shaders/room-pulse.fs' if args.preset == 'room-pulse' else 'shaders/room-tri-band.fs')
    controller_name = args.controller_name or ('Music - Room Pulse.json' if args.preset == 'room-pulse' else 'Music - Tri Band.json')
    template = json.loads(args.identity_template.read_bytes())
    profile = (replace_shader(template, shader.read_text(), args.detail) if args.shader_only
               else make_profile(template, shader.read_text(), controller_name,
                                 args.audio_device, args.detail, args.preset))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open('x', encoding='utf-8') as file:
        json.dump(profile, file, indent=2, ensure_ascii=False)
        file.write('\n')
    print('Profile file prepared without contacting OpenRGB. ' +
          ('Existing settings, including AutoStart, preserved.' if args.shader_only else
           'AutoStart disabled; verify canvas identity and audio endpoint before use.'))


if __name__ == '__main__':
    main()
