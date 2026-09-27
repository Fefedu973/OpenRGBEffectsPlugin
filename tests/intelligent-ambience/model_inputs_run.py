"""Build/run synthetic native model adapters. No model or capture is opened."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
for name in ('qt', 'jom', 'core'):
    p.add_argument('--' + name, type=Path, required=True)
a = p.parse_args()
here = Path(__file__).resolve().parent
out = here.parents[1] / 'build/intelligent-model-inputs'
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, PATH=str(a.qt.resolve() / 'bin') + os.pathsep + os.environ['PATH'])
subprocess.run([str(a.qt.resolve() / 'bin/qmake.exe'), str(here / 'model_inputs_tests.pro'),
                'CONFIG+=release', 'CONFIG-=debug_and_release', 'CONFIG-=build_all',
                'OPENRGB_ROOM_ROOT=' + a.core.resolve().as_posix()], cwd=out, env=env, check=True)
subprocess.run([str(a.jom.resolve()), '/J', '2'], cwd=out, env=env, check=True)
subprocess.run([str(out / 'model-inputs-tests.exe')], cwd=out, env=env, check=True, timeout=30)
