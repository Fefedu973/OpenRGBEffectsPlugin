"""Compile exact production SDK/profile dispatch methods with Qt thread fixtures."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--qt', type=Path, required=True)
p.add_argument('--core', type=Path, required=True)
p.add_argument('--without-marshalling', action='store_true', help='Require the direct-call regression to fail')
a = p.parse_args()
repo = Path(__file__).resolve().parents[2]
out = repo / 'build' / 'room-sdk-dispatch'
out.mkdir(parents=True, exist_ok=True)
source = (repo / 'OpenRGBEffectsPlugin.cpp').read_text(encoding='utf-8')

def extract(marker):
    start = source.index(marker)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

methods = [extract('template<typename Function>'),
           *[extract(marker) for marker in (
               'void OpenRGBEffectsPlugin::Unload(',
               'unsigned char* OpenRGBEffectsPlugin::OnSDKCommand(',
               'void OpenRGBEffectsPlugin::OnProfileAboutToLoad(',
               'void OpenRGBEffectsPlugin::OnProfileLoad(',
               'nlohmann::json OpenRGBEffectsPlugin::OnProfileSave(',
               'void OpenRGBEffectsPlugin::SettingsManagerUpdated(')]]
if a.without_marshalling:
    methods[0] = 'template<typename Function> void RunOnEffectUiThread(OpenRGBEffectTab*, Function function) { function(); }'
(out / 'production_dispatch.inc').write_text('\n\n'.join(methods), encoding='utf-8')
qt, core = a.qt.resolve(), a.core.resolve()
json_root = core / 'dependencies/json'
if not (json_root / 'nlohmann/json.hpp').exists():
    raise RuntimeError(f'Missing core JSON dependency: {json_root}')
env = dict(os.environ, PATH=str(qt / 'bin') + os.pathsep + os.environ['PATH'])
fixture = Path(__file__).with_name('dispatch_tests.cpp')
subprocess.run(['cl', '/nologo', '/EHsc', '/std:c++17', '/Zc:__cplusplus', '/permissive-', '/MD', '/O2',
                f'/I{qt / "include"}', f'/I{qt / "include/QtCore"}', f'/I{out}', f'/I{json_root}', str(fixture),
                f'/Fo:{out / "dispatch.obj"}', f'/Fe:{out / "dispatch-tests.exe"}',
                '/link', f'/LIBPATH:{qt / "lib"}', 'Qt6Core.lib'], env=env, cwd=out, check=True)
result = subprocess.run([str(out / 'dispatch-tests.exe')], env=env, cwd=out, timeout=10,
                        capture_output=True, text=True)
if a.without_marshalling:
    if result.returncode == 0 or 'QThread::currentThread() == thread()' not in result.stderr:
        raise RuntimeError('Direct dispatch did not reproduce the wrong-thread UI call')
    print('PASS: direct-call regression fails at the UI thread-affinity assertion')
else:
    print(result.stdout, end='')
    print(result.stderr, end='')
    result.check_returncode()
