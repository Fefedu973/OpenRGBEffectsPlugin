"""Compile and run pure synthetic PCM tests. Requires an MSVC developer shell.
No microphone, WASAPI loopback, OpenRGB process, plugin, GPU or devices are used.
"""
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[2]
output=root/'build/room-rhythm'
output.mkdir(parents=True,exist_ok=True)
subprocess.run(['cl','/nologo','/EHsc','/std:c++17','/O2','/MD','/DNOMINMAX','/utf-8',
                '/I'+str(root),str(Path(__file__).with_name('rhythm_tests.cpp')),
                '/Fo:'+str(output/'rhythm_tests.obj'),'/Fe:'+str(output/'rhythm_tests.exe')],
               cwd=output,check=True)
subprocess.run([str(output/'rhythm_tests.exe')],cwd=output,check=True,timeout=60)
