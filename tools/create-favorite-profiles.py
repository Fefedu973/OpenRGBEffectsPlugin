"""Prepare native favorite profiles from an existing room profile, without editing it."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import copy
import json
import math
from pathlib import Path


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def normalize(control, entry):
    default = control['default']
    if not entry or not entry.get('valid'):
        return default
    text = entry.get('color', entry.get('text', ''))
    kind = control['type']
    try:
        if kind == 'number':
            value = float(text)
            return min(control['max'], max(control['min'], value)) if math.isfinite(value) else default
        if kind == 'boolean':
            return text.lower() == 'true' if text.lower() in ('true', 'false') else default
        if kind == 'enum':
            return text if text in control['options'] else default
        if kind == 'color' and len(text) == 7 and text.startswith('#'):
            int(text[1:], 16)
            return text.lower()
    except (TypeError, ValueError, AttributeError):
        pass
    return default


def create(source, spec, preferences):
    profile = copy.deepcopy(source)
    profile['profile_name'] = spec.get('profile_prefix','Favori - ') + spec['title']
    profile['controllers'] = []
    profile.pop('base_color', None)
    effect = profile['plugins']['OpenRGB Effects Plugin']['Effects'][0]
    effect.update(EffectClassName='SignalFavorite.'+spec['id'], CustomName=spec['title'],
                  FPS=60, Speed=1000, AutoStart=True)
    settings = effect['CustomSettings']
    settings.pop('shader_program', None)
    settings.pop('shader_name', None)
    settings.update(preset=spec['id'], schema_version=1, width=800, height=500,
                    use_audio=False, show_rendering=False,
                    parameters={c['key']: normalize(c, preferences.get(c['key'])) for c in spec['controls']})
    return profile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--preferences', type=Path, help='Optional private read-only Qt preference export')
    args = parser.parse_args()
    source = read(args.base)
    if len(source['plugins']['OpenRGB Effects Plugin']['Effects']) != 1:
        raise ValueError('Base profile must contain exactly one canvas effect')
    saved = {e['id']: e['values'] for e in read(args.preferences)['effects']} if args.preferences else {}
    specs = sorted((Path(__file__).resolve().parents[1]/'Effects/SignalFavorites/presets').glob('*.json'))
    profiles = [create(source, spec := read(path), saved.get(spec['source_id'], {})) for path in specs]
    args.out.mkdir(parents=True, exist_ok=False)
    for profile in profiles:
        (args.out/(profile['profile_name']+'.json')).write_text(json.dumps(profile, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(f'Prepared {len(profiles)} native profiles; source layout and live settings untouched')


if __name__ == '__main__':
    main()
