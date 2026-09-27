"""Exercise production multipass GPU graph, without capture or devices."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse, os, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for key in ('qt','core'): p.add_argument('--'+key,type=Path,required=True)
a=p.parse_args(); repo=Path(__file__).resolve().parents[2]
out=repo/'build'/'room-shader-graph';out.mkdir(parents=True,exist_ok=True)
includes=[a.qt/'include',*[a.qt/'include'/m for m in ('QtCore','QtGui','QtOpenGL')],a.core,a.core/'dependencies/json',a.core/'RGBController',repo,repo/'Effects/Shaders']
cmd=['cl','/nologo','/EHsc','/std:c++17','/Zc:__cplusplus','/permissive-','/MD','/O2','/DNOMINMAX','/utf-8']
subprocess.run(cmd+['/I'+str(d) for d in includes]+[str(Path(__file__).with_name('graph_gpu_tests.cpp')),str(repo/'Effects/Shaders/ShaderPass.cpp'),str(repo/'Effects/Shaders/ShaderRenderGraph.cpp'),'/Fo:'+str(out)+os.sep,'/Fe:'+str(out/'graph-gpu.exe'),'/link','/LIBPATH:'+str(a.qt/'lib'),'Qt6Core.lib','Qt6Gui.lib','Qt6OpenGL.lib','opengl32.lib'],cwd=out,check=True)
env=dict(os.environ,PATH=str(a.qt/'bin')+os.pathsep+os.environ['PATH'])
subprocess.run([str(out/'graph-gpu.exe')],env=env,check=True,timeout=60)
