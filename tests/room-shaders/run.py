"""Build/test pure image math with Qt/MSVC; no GPU context or hardware access."""
import argparse
import os
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--qt', required=True, type=Path)
a=p.parse_args()
repo=Path(__file__).resolve().parents[2]
out=repo/'build'/'room-shaders'
out.mkdir(parents=True,exist_ok=True)
qt=a.qt.resolve()
env=os.environ.copy()
env['PATH']=str(qt/'bin')+os.pathsep+env['PATH']
subprocess.run(['cl','/nologo','/EHsc','/std:c++17','/Zc:__cplusplus','/permissive-',
    '/O2','/MD','/utf-8',f'/I{qt / "include"}',f'/I{qt / "include/QtCore"}',
    f'/I{qt / "include/QtGui"}',f'/I{repo / "Effects/Shaders"}',
    str(Path(__file__).with_name('canvas_tests.cpp')),f'/Fo:{out / "canvas.obj"}',
    f'/Fe:{out / "canvas-tests.exe"}','/link',f'/LIBPATH:{qt / "lib"}',
    'Qt6Core.lib','Qt6Gui.lib'],env=env,cwd=out,check=True)
subprocess.run([str(out/'canvas-tests.exe')],env=env,cwd=out,check=True)
