"""Build-only unless --run explicitly requests current Better read-only appearance."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--qt',type=Path,required=True)
parser.add_argument('--core',type=Path,required=True)
parser.add_argument('--run',action='store_true')
parser.add_argument('--scene-cycle',action='store_true',help='Explicit transient scene-control test; requires --run, restores via lease release')
parser.add_argument('--descriptor',type=Path)
args=parser.parse_args()
if args.descriptor and not args.descriptor.is_absolute():parser.error('Descriptor path must be absolute')
if args.scene_cycle and not args.run:parser.error('--scene-cycle requires explicit --run')
root=Path(__file__).resolve().parents[2];out=root/'build/room-better-live';out.mkdir(parents=True,exist_ok=True)
qrc=ET.Element('RCC');resource=ET.SubElement(qrc,'qresource',{'prefix':'/'})
assets=sorted((root/'shaders/BetterCapture').glob('*'))
for path in assets:
    if path.suffix in ('.fs','.glsl'):
        node=ET.SubElement(resource,'file',{'alias':path.relative_to(root).as_posix()});node.text=path.as_posix()
ET.ElementTree(qrc).write(out/'appearance.qrc',encoding='utf-8',xml_declaration=True)
subprocess.run([str(args.qt/'bin/rcc.exe'),str(out/'appearance.qrc'),'-o',str(out/'qrc_appearance.cpp')],check=True)
sources=[Path(__file__).with_name('probe.cpp'),out/'qrc_appearance.cpp']
sources += [root/'ScreenSources'/(name+'.cpp') for name in ('BetterDiscovery','BetterFrameSource','ScreenSource','BetterAppearanceInput')]
sources += [root/'Effects/BetterCapture/Appearance.cpp',root/'Effects/Shaders/ShaderRenderGraph.cpp',root/'Effects/Shaders/ShaderPass.cpp']
if args.scene_cycle:
    sources=[Path(__file__).with_name('scene_cycle.cpp')]+[root/'ScreenSources'/(name+'.cpp') for name in ('BetterDiscovery','BetterFrameSource','ScreenSource')]
includes=[root,args.core,args.core/'RGBController',args.core/'dependencies/json',root/'Effects/Shaders',args.qt/'include']
includes += [args.qt/'include'/('Qt'+part) for part in ('Core','Gui','Network','OpenGL')]
cmd=['cl','/nologo','/EHsc','/std:c++17','/Zc:__cplusplus','/permissive-','/MD','/O2','/DNOMINMAX','/utf-8']
cmd += ['/I'+str(p) for p in includes]+[str(p) for p in sources]
exe=out/('scene-cycle.exe' if args.scene_cycle else 'probe.exe')
cmd += ['/Fo:'+str(out)+os.sep,'/Fe:'+str(exe),'/link','/LIBPATH:'+str(args.qt/'lib'),'Qt6Core.lib','Qt6Gui.lib','Qt6Network.lib','Qt6OpenGL.lib','opengl32.lib','advapi32.lib']
subprocess.run(cmd,cwd=out,check=True)
provenance={'effects_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
            'production_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources+assets if p.is_relative_to(root) and 'build' not in p.relative_to(root).parts}}
if args.run:
    env=dict(os.environ,PATH=str(args.qt/'bin')+os.pathsep+os.environ['PATH'])
    command=[str(exe)]
    if args.scene_cycle:
        private_report=out/('scene-cycle-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'.json')
        command+=['--report',str(private_report)]
    if args.descriptor:command+=['--descriptor',str(args.descriptor)]
    result=subprocess.run(command,cwd=out,env=env,capture_output=True,text=True,timeout=35 if args.scene_cycle else 25)
    lines=[line for line in result.stdout.splitlines() if line.startswith('{')]
    if not lines:raise SystemExit('No JSON diagnostic returned; no producer data was logged.')
    report=json.loads(lines[-1]);report.update(provenance)
    if args.scene_cycle:
        if private_report.exists():
            saved=json.loads(private_report.read_text(encoding='utf-8'));saved.update(provenance)
            private_report.write_text(json.dumps(saved,indent=2,ensure_ascii=False),encoding='utf-8')
        print(json.dumps(report,indent=2,ensure_ascii=False))
        print('Private scene report:',private_report)
        raise SystemExit(result.returncode)
    (out/'report.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
    print(json.dumps(report,indent=2,ensure_ascii=False))
    raise SystemExit(result.returncode)
print('Built without live access:',out/'probe.exe')
