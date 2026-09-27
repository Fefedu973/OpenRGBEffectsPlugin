"""MSVC: test PCM/WASAPI metadata and real GL shader rendering, no audio stream."""
import argparse
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--qt', type=Path, required=True)
p.add_argument('--openrgb-root', type=Path, required=True)
p.add_argument('--profile', type=Path, required=True)
a = p.parse_args()
repo = Path(__file__).resolve().parents[2]
out = repo/'build/room-audio'
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, PATH=str(a.qt/'bin')+os.pathsep+os.environ['PATH'])
common = ['cl','/nologo','/EHsc','/std:c++17','/Zc:__cplusplus','/permissive-','/MD','/O2','/DNOMINMAX','/utf-8']
subprocess.run(common+['/I'+str(repo),str(Path(__file__).with_name('audio_tests.cpp')),
                      '/Fo:'+str(out/'audio_tests.obj'),'/Fe:'+str(out/'audio_tests.exe'),
                      '/link','ole32.lib','propsys.lib'],check=True)
subprocess.run([str(out/'audio_tests.exe')],check=True,timeout=10,env=env)
includes = [a.qt/'include',a.qt/'include/QtCore',a.qt/'include/QtGui',a.qt/'include/QtOpenGL',
            a.openrgb_root/'dependencies/json',a.openrgb_root/'RGBController',repo/'Effects/Shaders']
subprocess.run(common+['/I'+str(path) for path in includes]+
               [str(Path(__file__).with_name('music_shader_tests.cpp')),str(repo/'Effects/Shaders/ShaderPass.cpp'),
                str(repo/'Effects/Shaders/ShaderProgram.cpp'),'/Fo:'+str(out)+os.sep,
                '/Fe:'+str(out/'music_shader_tests.exe'),'/link','/LIBPATH:'+str(a.qt/'lib'),
                'Qt6Core.lib','Qt6Gui.lib','Qt6OpenGL.lib','opengl32.lib'],check=True,cwd=out)
subprocess.run([str(out/'music_shader_tests.exe'),str(a.profile.resolve()),str(out)],check=True,timeout=20,env=env,cwd=out)
