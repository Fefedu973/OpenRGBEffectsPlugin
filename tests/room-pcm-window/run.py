"""Compile/test the real Windows PCM publication path without opening audio."""
# SPDX-License-Identifier: GPL-2.0-or-later
from pathlib import Path
import subprocess

repo = Path(__file__).resolve().parents[2]
out = repo / 'build/room-pcm-window'
out.mkdir(parents=True, exist_ok=True)
for source in (Path(__file__).with_name('pcm_window_tests.cpp'),
               repo / 'tests/room-audio/audio_tests.cpp',
               repo / 'tests/room-audio/packet_capture_tests.cpp'):
    exe = out / (source.stem + '.exe')
    subprocess.run(['cl', '/nologo', '/EHsc', '/std:c++17', '/Zc:__cplusplus',
                    '/permissive-', '/MD', '/O2', '/DNOMINMAX', '/utf-8',
                    '/I' + str(repo), str(source), '/Fo:' + str(out / (source.stem + '.obj')),
                    '/Fe:' + str(exe), '/link', 'ole32.lib', 'propsys.lib'],
                   cwd=out, check=True)
    subprocess.run([str(exe)], cwd=out, check=True, timeout=40)
