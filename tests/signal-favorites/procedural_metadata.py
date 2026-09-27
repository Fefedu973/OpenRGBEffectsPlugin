"""Audit all 43 control contracts; optionally verify the locally installed references."""
# SPDX-License-Identifier: GPL-2.0-or-later
import argparse, hashlib, json
from html.parser import HTMLParser
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--cache',type=Path,help='Optional local effects cache (read only; source is never copied)')
a=p.parse_args();repo=Path(__file__).resolve().parents[2]
manifest=json.loads(Path(__file__).with_name('procedural-family.metadata.json').read_text(encoding='utf-8'))
class Metas(HTMLParser):
 def __init__(self):super().__init__();self.controls=[]
 def handle_starttag(self,tag,attrs):
  row=dict(attrs)
  if tag=='meta' and 'property' in row:self.controls.append(row)
def normalize(c):
 t=c['type'];v=c['default'];out={'key':c['property'],'label':c['label'],'type':'enum' if t=='combobox' else t,'default':v}
 if t=='number':out['default']=float(v);out['step']=1
 if t=='boolean':out['default']=v not in ('0','false','False','')
 if t=='combobox':out['options']=c['values'].split(',')
 for k in ('min','max'):
  if k in c:out[k]=float(c[k])
 return out
count=0
for item in manifest:
 spec=json.loads((repo/'Effects/SignalFavorites/presets'/f"{item['slug']}.json").read_text(encoding='utf-8'))
 assert spec['id']==item['id'] and spec['source_id']==item['source_id']
 assert spec['controls']==item['controls'],item['id']
 shader=(repo/'shaders/SignalFavorites'/f"{item['slug']}.fs").read_text(encoding='utf-8')
 assert 'mainImage' in shader and 'prState' in shader
 if a.cache:
  raw=(a.cache/item['source_id']/'effect.html').read_bytes()
  assert hashlib.sha256(raw).hexdigest()==item['source_sha256'],item['id']+' reference changed'
  html=Metas();html.feed(raw.decode('utf-8-sig'))
  assert [normalize(c) for c in html.controls]==spec['controls'],item['id']+' control mismatch'
 count+=len(spec['controls'])
assert count==43 and len(manifest)==5
print(f'PASS 5 presets / {count} exact control contracts'+(' / 5 installed source hashes' if a.cache else ''))
