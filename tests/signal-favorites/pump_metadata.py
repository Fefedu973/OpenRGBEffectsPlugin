"""Check the twenty recorded Pump controls and an optional installed reference."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path

REFERENCE_SHA256='9b0d5159d7ef5fa6a75ae709a676bd76b5cc37d6a3ceb2d2c697dc4d29a9ddc9'
class Metadata(HTMLParser):
    def __init__(self):super().__init__();self.controls=[]
    def handle_starttag(self,tag,attrs):
        a=dict(attrs)
        if tag!='meta' or 'property' not in a:return
        kind={'combobox':'enum'}.get(a['type'],a['type']);value=a['default']
        if kind=='boolean':value=value.lower() in ('true','1')
        elif kind=='number':value=float(value)
        c=dict(key=a['property'],label=a['label'],type=kind,default=value)
        for k in ('min','max','step'):
            if k in a:c[k]=float(a[k])
        if kind=='number':c.setdefault('step',1)
        if kind=='enum':c['options']=a['values'].split(',')
        self.controls.append(c)

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--reference',type=Path,help='Read installed effect.html in place; never copy it.')
a=p.parse_args();repo=Path(__file__).resolve().parents[2]
spec=json.loads((repo/'Effects/SignalFavorites/presets/pump-up-beats.json').read_text())
assert len(spec['controls'])==20
assert len({c['key'] for c in spec['controls']})==20
assert spec['source_id']=='-MhwiL3cQgnjhqzS0g2L'
assert spec['feedback'] and spec['audioReactive'] and spec['keyboard_reactive']
# ScreenDominant is a named native extension, appended to retain all original
# enum indices/defaults. The twenty original controls remain otherwise exact.
controls=json.loads(json.dumps(spec['controls']))
color=next(c for c in controls if c['key']=='colorStyle')
assert color['options'].pop()=='ScreenDominant'
assert spec['screenDominant'] and not spec.get('screenReactive',False)
# The optional source check compares actual labels, defaults, limits and enums,
# not just a count of controls. The source is not redistributed in this repo.
if a.reference:
    data=a.reference.read_bytes();assert hashlib.sha256(data).hexdigest()==REFERENCE_SHA256
    parser=Metadata();parser.feed(data.decode('utf-8-sig'))
    assert parser.controls==controls,'original control differs from the audited reference'
print('PASS: twenty original controls'+(' match the audited reference SHA256' if a.reference else ' are valid')+', plus explicit ScreenDominant enum extension')
