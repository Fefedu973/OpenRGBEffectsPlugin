"""Compile/render the native appearance graph against existing synthetic Better fixtures."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for key in ('qt','core','better'):p.add_argument('--'+key,type=Path,required=True)
a=p.parse_args();repo=Path(__file__).resolve().parents[2];out=repo/'build'/'room-better-appearance';out.mkdir(parents=True,exist_ok=True)
includes=[repo,a.core,a.core/'RGBController',repo/'Effects/Shaders',a.core/'dependencies/json',a.qt/'include',*[a.qt/'include'/module for module in ('QtCore','QtGui','QtOpenGL')]]
cmd=['cl','/nologo','/EHsc','/std:c++17','/Zc:__cplusplus','/permissive-','/MD','/O2','/DNOMINMAX','/utf-8']
subprocess.run(cmd+['/I'+str(x) for x in includes]+[str(Path(__file__).with_name('appearance_tests.cpp')),str(repo/'Effects/BetterCapture/Appearance.cpp'),str(repo/'Effects/Shaders/ShaderRenderGraph.cpp'),str(repo/'Effects/Shaders/ShaderPass.cpp'),'/Fo:'+str(out)+os.sep,'/Fe:'+str(out/'appearance-tests.exe'),'/link','/LIBPATH:'+str(a.qt/'lib'),'Qt6Core.lib','Qt6Gui.lib','Qt6OpenGL.lib','opengl32.lib'],cwd=out,check=True)
env=dict(os.environ,PATH=str(a.qt/'bin')+os.pathsep+os.environ['PATH'])
subprocess.run([str(out/'appearance-tests.exe'),str(repo),str(a.better/'tests/fixtures/native-rendering-v1'),str(out)],env=env,check=True,timeout=120)
