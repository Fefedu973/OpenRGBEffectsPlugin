"""Synthetic music shader test: no capture, host, audio device or controller.
Run in an x64 MSVC environment matching --qt. Uses production ShaderProgram.
"""
from pathlib import Path
import argparse
import os
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--qt', type=Path, required=True)
p.add_argument('--core', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
out = root / 'build/intelligent-ambience-music-gpu'
out.mkdir(parents=True, exist_ok=True)
qt, core = a.qt.resolve(), a.core.resolve()
includes = [root, root/'Effects/Shaders', core, core/'RGBController', core/'dependencies/json',
            qt/'include', *(qt/'include'/x for x in ('QtCore','QtGui','QtOpenGL'))]
sources = [Path(__file__).with_name('music_gpu_tests.cpp'),
           root/'Effects/IntelligentAmbience/MusicDirector.cpp',
           root/'Effects/Shaders/ShaderPass.cpp', root/'Effects/Shaders/ShaderProgram.cpp']
subprocess.run(['cl','/nologo','/EHsc','/std:c++17','/Zc:__cplusplus','/permissive-',
                '/MD','/O2','/DNOMINMAX','/utf-8', *('/I'+str(x) for x in includes),
                *(str(x) for x in sources), '/Fe:'+str(out/'music_gpu_tests.exe'),
                '/link','/LIBPATH:'+str(qt/'lib'),'Qt6Core.lib','Qt6Gui.lib','Qt6OpenGL.lib','opengl32.lib'],
               cwd=out, check=True)
env = dict(os.environ, PATH=str(qt/'bin')+os.pathsep+os.environ['PATH'], QT_QPA_PLATFORM='windows')
subprocess.run([str(out/'music_gpu_tests.exe'),str(root/'shaders/IntelligentAmbience/music.fs')],
               cwd=out, env=env, check=True, timeout=60)
