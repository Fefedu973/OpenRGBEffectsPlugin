"""Build and run the real Effects DLL UI test with an empty API5 host."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import os
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--qt',type=Path,required=True)
p.add_argument('--jom',type=Path,required=True)
p.add_argument('--core',type=Path,required=True)
p.add_argument('--dll',type=Path,required=True)
a=p.parse_args()
here=Path(__file__).resolve().parent
out=here.parents[1]/'build/signal-favorites-ui'
out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,PATH=str(a.qt.resolve()/'bin')+os.pathsep+os.environ['PATH'])
subprocess.run([str(a.qt.resolve()/'bin/qmake.exe'),str(here/'ui_favorites.pro'),
                'CONFIG+=release','CONFIG-=debug_and_release','CONFIG-=build_all',
                'OPENRGB_ROOM_ROOT='+a.core.resolve().as_posix()],cwd=out,env=env,check=True)
subprocess.run([str(a.jom.resolve()),'/J','4'],cwd=out,env=env,check=True)
subprocess.run([str(out/'ui-favorites-test.exe'),str(a.dll.resolve())],
               cwd=out,env=env,check=True,timeout=40)
