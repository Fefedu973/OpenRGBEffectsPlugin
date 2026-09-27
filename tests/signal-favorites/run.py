"""Render shipped favorites and control boundaries using the production GPU code."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import json
import os
import subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for key in ('qt','openrgb-root'):
    p.add_argument('--'+key,required=True,type=Path)
a=p.parse_args()
repo=Path(__file__).resolve().parents[2]
out=repo/'build/signal-favorites'; out.mkdir(parents=True,exist_ok=True)
includes=[a.qt/'include',a.qt/'include/QtCore',a.qt/'include/QtGui',a.qt/'include/QtOpenGL',
          a.openrgb_root/'dependencies/json',a.openrgb_root/'RGBController',repo/'Effects/Shaders',repo/'Effects/SignalFavorites',repo/'Audio']
cmd=['cl','/nologo','/EHsc','/std:c++17','/Zc:__cplusplus','/permissive-','/MD','/O2','/DNOMINMAX','/utf-8']
subprocess.run(cmd+['/I'+str(d) for d in includes]+
    [str(Path(__file__).with_name('render_favorites.cpp')),str(repo/'Effects/Shaders/ShaderPass.cpp'),
     str(repo/'Effects/Shaders/ShaderProgram.cpp'),'/Fo:'+str(out)+os.sep,'/Fe:'+str(out/'render_favorites.exe'),
     '/link','/LIBPATH:'+str(a.qt/'lib'),'Qt6Core.lib','Qt6Gui.lib','Qt6OpenGL.lib','opengl32.lib'],cwd=out,check=True)
env=dict(os.environ,PATH=str(a.qt/'bin')+os.pathsep+os.environ['PATH'])
subprocess.run([str(out/'render_favorites.exe'),str(repo),str(out)],env=env,check=True,timeout=140)
report=json.loads((out/'gpu-validation.json').read_text(encoding='utf-8'))
print(json.dumps({
    'presets':len(report),
    'analytic_color_checkpoints':sum(len(r.get('color_checkpoints',[])) for r in report),
    'timing_scope':'800x500 production shader draw plus synchronous FBO readback; not device FPS',
    'render_readback':[{'id':r['id'],**r['render_readback']} for r in report],
},indent=2))
