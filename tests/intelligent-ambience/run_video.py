"""Synthetic video CPU tests and benchmark. Run in an x64 MSVC developer shell.
No Qt, capture, model, host or controller access.
"""
# SPDX-License-Identifier: GPL-2.0-or-later
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'build/intelligent-ambience-video'
out.mkdir(parents=True, exist_ok=True)
subprocess.run([
    'cl', '/nologo', '/EHsc', '/std:c++17', '/O2', '/W4', '/WX',
    str(root / 'Effects/IntelligentAmbience/VideoEngine.cpp'),
    str(Path(__file__).with_name('video_cpu_tests.cpp')),
    '/Fe:' + str(out / 'video_cpu_tests.exe'),
], cwd=out, check=True)
subprocess.run([str(out / 'video_cpu_tests.exe')], cwd=out, check=True, timeout=30)
