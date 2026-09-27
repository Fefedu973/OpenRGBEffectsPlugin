"""Compile the production ResourceManager callback with a Qt thread fixture.

No plugin, OpenRGB process or hardware is started. --without-fix must miss the
same-thread callback or time out; Qt versions can reject self-blocking delivery.
"""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--qt', type=Path, required=True)
p.add_argument('--without-fix', action='store_true')
a = p.parse_args()
repo = Path(__file__).resolve().parents[2]
out = repo / 'build' / 'room-lifecycle'
out.mkdir(parents=True, exist_ok=True)
source = (repo / 'OpenRGBEffectsPlugin.cpp').read_text(encoding='utf-8')
start = source.index('void OpenRGBEffectsPlugin::ResourceManagerUpdated(')
opening = source.index('{', start)
depth = 1
end = opening + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
method = source[start:end]
if a.without_fix:
    method = method.replace('QThread::currentThread() == thread()\n'
                            '                    ? Qt::DirectConnection : Qt::BlockingQueuedConnection',
                            'Qt::BlockingQueuedConnection')
    if 'Qt::DirectConnection' in method:
        raise RuntimeError('Former callback reproduction no longer matches production')
(out / 'production_callback.inc').write_text(method, encoding='utf-8')
qt = a.qt.resolve()
env = dict(os.environ, PATH=str(qt / 'bin') + os.pathsep + os.environ['PATH'])
fixture = Path(__file__).with_name('callback_tests.cpp')
subprocess.run([str(qt / 'bin/moc.exe'), str(fixture), '-o', str(out / 'moc_callback_tests.cpp')], check=True)
subprocess.run(['cl', '/nologo', '/EHsc', '/std:c++17', '/Zc:__cplusplus', '/permissive-', '/MD', '/O2',
                f'/I{qt / "include"}', f'/I{qt / "include/QtCore"}', f'/I{out}', str(fixture),
                f'/Fo:{out / "callback.obj"}', f'/Fe:{out / "callback-tests.exe"}',
                '/link', f'/LIBPATH:{qt / "lib"}', 'Qt6Core.lib'], env=env, cwd=out, check=True)
try:
    result = subprocess.run([str(out / 'callback-tests.exe')], env=env, cwd=out,
                            timeout=5, capture_output=True, text=True)
except subprocess.TimeoutExpired:
    if not a.without_fix:
        raise
    print('PASS: former production callback reproduces same-thread deadlock (bounded timeout)')
else:
    print(result.stdout, end='')
    print(result.stderr, end='')
    if a.without_fix:
        if result.returncode != 2 or 'GUI result calls=0 wrong_thread=0' not in result.stderr:
            raise RuntimeError('Former callback did not reproduce rejected GUI delivery')
        print('PASS: former callback does not deliver the same-thread controller remap')
    else:
        result.check_returncode()
