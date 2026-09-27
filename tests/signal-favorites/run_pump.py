"""Build/run production ShaderPass audio and Pump tests on a local OpenGL context."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import os
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--qt',type=Path,required=True)
p.add_argument('--openrgb-root',type=Path,required=True)
a=p.parse_args()
repo=Path(__file__).resolve().parents[2]
out=repo/'build/pump-up-beats'
out.mkdir(parents=True,exist_ok=True)
includes=[a.qt/'include',a.qt/'include/QtCore',a.qt/'include/QtGui',a.qt/'include/QtOpenGL',
          a.openrgb_root/'dependencies/json',a.openrgb_root/'RGBController',repo/'Effects/Shaders']
cmd=['cl','/nologo','/EHsc','/std:c++17','/Zc:__cplusplus','/permissive-','/MD','/O2','/DNOMINMAX','/utf-8']
subprocess.run(cmd+['/I'+str(d) for d in includes]+[
    str(Path(__file__).with_name('pump_gpu_tests.cpp')),
    str(repo/'Effects/Shaders/ShaderPass.cpp'),str(repo/'Effects/Shaders/ShaderProgram.cpp'),
    '/Fo:'+str(out)+os.sep,'/Fe:'+str(out/'pump_gpu_tests.exe'),
    '/link','/LIBPATH:'+str(a.qt/'lib'),'Qt6Core.lib','Qt6Gui.lib','Qt6OpenGL.lib','opengl32.lib'],cwd=out,check=True)
subprocess.run(cmd+['/I'+str(repo/'Audio'),'/I'+str(repo/'Effects/SignalFavorites'),'/I'+str(a.openrgb_root/'dependencies/json'),str(Path(__file__).with_name('pump_dynamics_tests.cpp')),'/Fo:'+str(out/'pump_dynamics_tests.obj'),'/Fe:'+str(out/'pump_dynamics_tests.exe')],cwd=out,check=True)
subprocess.run([str(out/'pump_dynamics_tests.exe')],check=True,timeout=30)
env=dict(os.environ,PATH=str(a.qt/'bin')+os.pathsep+os.environ['PATH'])
subprocess.run([str(out/'pump_gpu_tests.exe'),str(repo),str(out)],env=env,check=True,timeout=140)
