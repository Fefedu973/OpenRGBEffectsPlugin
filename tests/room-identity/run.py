"""Exercise production identity matching and serialization without devices."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--openrgb-root', type=Path, required=True)
p.add_argument('--without-fix', action='store_true')
a = p.parse_args()
repo = Path(__file__).resolve().parents[2]
out = repo / 'build/room-identity'
out.mkdir(parents=True, exist_ok=True)
source = (repo / 'ControllerZone.cpp').read_text(encoding='utf-8')

def extract(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

method = extract('bool ControllerZone::matches_json(')
if a.without_fix:
    anchor = 'return( location_matches'
    if anchor not in method:
        raise RuntimeError('Production matcher changed: update regression fixture')
    method = method.replace(anchor, 'return( controller->GetVersion() == controller_zone_json["version"] && location_matches')
(out / 'production_identity.inc').write_text(
    method + '\n' + extract('nlohmann::json ControllerZone::to_json()'), encoding='utf-8')
subprocess.run(['cl', '/nologo', '/EHsc', '/std:c++17', '/MD', '/O2',
                f'/I{a.openrgb_root.resolve() / "dependencies/json"}', f'/I{out}',
                str(Path(__file__).with_name('identity_tests.cpp')),
                f'/Fo:{out / "identity.obj"}', f'/Fe:{out / "identity-tests.exe"}'],
               cwd=out, check=True)
result = subprocess.run([str(out / 'identity-tests.exe')], capture_output=True, text=True, timeout=5)
print(result.stdout, end='')
print(result.stderr, end='')
if a.without_fix:
    if result.returncode == 0 or 'updated plugin must retain the saved selection' not in result.stderr:
        raise RuntimeError('Former version match did not reproduce the update regression')
    print('PASS: former version match rejects an otherwise identical saved controller')
else:
    result.check_returncode()
