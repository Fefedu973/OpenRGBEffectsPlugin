"""Stage three native Effects profiles using existing Full Scale/source settings.

No live file is written and no application or capture is started. Model packages
are intentionally disabled. Screen geometry still needs user calibration.
"""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import copy
import json
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--config', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
config, output = a.config.resolve(), a.output.resolve()
if output == config or config in output.parents:
    p.error('Output must be a staging directory outside the live configuration')

def read(name):
    return json.loads((config / 'profiles' / name).read_text(encoding='utf-8-sig'))

template = read('Ambilight - Main screen.json')
audio = read('Music - Room Pulse.json')['plugins']['OpenRGB Effects Plugin']['Effects'][0]['CustomSettings']['audio_settings']
output.mkdir(parents=True, exist_ok=True)
for mode, suffix in enumerate(('Video', 'Musique', 'Hybride')):
    profile = copy.deepcopy(template)
    name = 'IA - ' + suffix
    profile['profile_name'] = name
    effect = profile['plugins']['OpenRGB Effects Plugin']['Effects'][0]
    effect.update(EffectClassName='IntelligentAmbience', CustomName=name, FPS=60)
    assert len(effect['ControllerZones']) == 1
    zone = effect['ControllerZones'][0]
    assert zone['name'] == 'Full Scale.json' and zone['zone_idx'] == 0
    assert zone['serial'] == 'VISUAL_MAP_VISUAL_CONTROLLER_SERIAL'
    source = copy.deepcopy(effect['CustomSettings']['screen_source'])
    source['follow_better_appearance'] = False
    effect['CustomSettings'] = dict(
        width=800, height=500, publish_frame=False, frame_channel='room-intelligent',
        zone_regions=[], show_rendering=False, invert_time=False,
        use_audio=mode != 0, audio_settings=copy.deepcopy(audio), rhythm_tracking=True,
        screen_source=source, intelligent_schema=1,
        intelligent=dict(mode=mode, demo=False, predictive=True, persistence=1,
                         strength=.75, hybrid=.25, screen=[80, 55, 160, 90],
                         inference_fps=10,
                         video_model=dict(enabled=False, manifest=''),
                         music_model=dict(enabled=False, manifest='')))
    path = output / (name + '.json')
    path.write_text(json.dumps(profile, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(path.name)
