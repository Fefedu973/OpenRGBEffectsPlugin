"""Compile actual Windows audio capture/PCM publication tests; never open a stream."""
# SPDX-License-Identifier: GPL-2.0-or-later
import os
from pathlib import Path
import subprocess

repo=Path(__file__).resolve().parents[2]
out=repo/'build/room-audio-capture'
out.mkdir(parents=True,exist_ok=True)
for name in ('audio_tests','packet_capture_tests'):
    subprocess.run(['cl','/nologo','/EHsc','/std:c++17','/Zc:__cplusplus','/permissive-',
                    '/MD','/O2','/DNOMINMAX','/utf-8','/I'+str(repo),
                    str(Path(__file__).with_name(name+'.cpp')),
                    '/Fo:'+str(out/(name+'.obj')),'/Fe:'+str(out/(name+'.exe')),
                    '/link','ole32.lib','propsys.lib'],check=True,cwd=out)
    subprocess.run([str(out/(name+'.exe'))],check=True,timeout=30,cwd=out)
