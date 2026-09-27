"""Compile and render six local presets with the production Qt/OpenGL renderer."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import os
import subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for key in ('qt','openrgb-root','profiles'):
    p.add_argument('--'+key,required=True,type=Path)
a=p.parse_args()
repo=Path(__file__).resolve().parents[2]
out=repo/'build/room-scenes'; out.mkdir(parents=True,exist_ok=True)
includes=[a.qt/'include',a.qt/'include/QtCore',a.qt/'include/QtGui',a.qt/'include/QtOpenGL',
          a.openrgb_root/'dependencies/json',a.openrgb_root/'RGBController',repo/'Effects/Shaders']
cmd=['cl','/nologo','/EHsc','/std:c++17','/Zc:__cplusplus','/permissive-','/MD','/O2','/DNOMINMAX','/utf-8']
subprocess.run(cmd+['/I'+str(d) for d in includes]+
    [str(Path(__file__).with_name('render_scenes.cpp')),str(repo/'Effects/Shaders/ShaderPass.cpp'),
     str(repo/'Effects/Shaders/ShaderProgram.cpp'),'/Fo:'+str(out)+os.sep,'/Fe:'+str(out/'render_scenes.exe'),
     '/link','/LIBPATH:'+str(a.qt/'lib'),'Qt6Core.lib','Qt6Gui.lib','Qt6OpenGL.lib','opengl32.lib'],cwd=out,check=True)
env=dict(os.environ,PATH=str(a.qt/'bin')+os.pathsep+os.environ['PATH'])
subprocess.run([str(out/'render_scenes.exe'),str(a.profiles.resolve()),str(out)],env=env,check=True,timeout=25)
