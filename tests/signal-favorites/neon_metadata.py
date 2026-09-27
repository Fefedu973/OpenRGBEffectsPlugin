"""Verify Neon controls, optionally against the installed reference in place."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path

REFERENCE_SHA256='0e7eab87de0209e8c9128571cdd6fc25d2d960313a3e287d0f472299121a1f71'
class Metadata(HTMLParser):
    def __init__(self):
        super().__init__();self.controls=[]
    def handle_starttag(self,tag,attrs):
        a=dict(attrs)
        if tag!='meta' or 'property' not in a:return
        kind=a['type'];value=a['default']
        if kind=='boolean':value=value.lower() in ('true','1')
        elif kind=='number':value=float(value)
        c=dict(key=a['property'],label=a['label'],type=kind,default=value)
        for k in ('min','max','step'):
            if k in a:c[k]=float(a[k])
        if kind=='number':c.setdefault('step',1)
        self.controls.append(c)

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--reference',type=Path,help='Optional installed effect.html; read in place, never copied.')
a=p.parse_args()
repo=Path(__file__).resolve().parents[2]
spec=json.loads((repo/'Effects/SignalFavorites/presets/NeonNebula.json').read_text(encoding='utf-8'))
colors=['#360059','#005159','#590046','#00baff','#f500ff','#0046ff']
expected=[dict(key=f'color{i+1}',label=f'Color {i+1}',type='color',default=v,min=0,max=360) for i,v in enumerate(colors)]
expected += [dict(key='speedRaw',label='Speed',type='number',default=5,min=1,max=10,step=1),
             dict(key='tapEffect',label='Keypress effect',type='boolean',default=True)]
assert spec['controls']==expected
assert spec['feedback'] is True and spec['tap_speed_key']=='speedRaw'
assert spec['source_id']=='-Mjb1GEcNJhwbQxjTw-z'
if a.reference:
    data=a.reference.read_bytes()
    assert hashlib.sha256(data).hexdigest()==REFERENCE_SHA256,'reference changed; audit required'
    parser=Metadata();parser.feed(data.decode('utf-8-sig'))
    assert parser.controls==expected,'preset controls differ from source declarations'
print('PASS: all eight Neon controls'+(' match the audited installed reference SHA256' if a.reference else ' match the recorded metadata'))
