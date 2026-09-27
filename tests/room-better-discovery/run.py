"""Compile the real Better discovery worker and exercise synthetic loopback HTTP only."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--qt',required=True,type=Path);a=p.parse_args()
repo=Path(__file__).resolve().parents[2];out=repo/'build/better-discovery';out.mkdir(parents=True,exist_ok=True)
include=[a.qt/'include',a.qt/'include/QtCore',a.qt/'include/QtNetwork',repo/'ScreenSources']
cmd=['cl','/nologo','/EHsc','/std:c++17','/Zc:__cplusplus','/permissive-','/MD','/O2','/DNOMINMAX','/utf-8']
subprocess.run(cmd+['/I'+str(p) for p in include]+[str(Path(__file__).with_name('discovery_tests.cpp')),str(repo/'ScreenSources/BetterDiscovery.cpp'),'/Fo:'+str(out)+os.sep,'/Fe:'+str(out/'discovery_tests.exe'),'/link','/LIBPATH:'+str(a.qt/'lib'),'Qt6Core.lib','Qt6Network.lib'],cwd=out,check=True)
env=dict(os.environ,PATH=str(a.qt/'bin')+os.pathsep+os.environ['PATH']);subprocess.run([str(out/'discovery_tests.exe')],env=env,check=True,timeout=40)
