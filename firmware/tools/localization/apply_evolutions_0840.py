#!/usr/bin/env python3
"""Add reviewed .40 translations and compact JSON without changing old values."""
from pathlib import Path
import hashlib,json,zipfile
ROOT=Path(__file__).resolve().parents[2]
SKETCH=ROOT/'Tab5OS_Arduino_v0.8.40_Ecriture'
EXPECTED='a4274b8fb44642306dc9c78e212d58d9064147d9bbdb0e624b4afb612af7d5ff'
def main():
 import argparse
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--baseline',type=Path,required=True);a=ap.parse_args()
 assert hashlib.sha256(a.baseline.read_bytes()).hexdigest()==EXPECTED
 delta=json.loads((ROOT/'tools/localization/evolutions-delta-0840.json').read_text())
 assert len(delta)==14
 rows=[]
 with zipfile.ZipFile(a.baseline) as z:
  for locale,added in delta.items():
   p=SKETCH/'sdcard/System/languages'/(locale+'.json');data=json.loads(p.read_text())
   original=json.loads(z.read('Tab5OS_Ecriture_v0.8.39/Tab5OS_Arduino_v0.8.39_Ecriture/sdcard/System/languages/'+locale+'.json'))
   assert {k:v for k,v in data.items() if k!='translations'}=={k:v for k,v in original.items() if k!='translations'}
   assert all(data['translations'].get(k)==v for k,v in original['translations'].items())
   for k,v in added.items():
    assert isinstance(v,str) and v
    if k not in data['translations']:data['translations'][k]=v
    elif k not in original['translations']:assert data['translations'][k]==v,(locale,k,'unreviewed replacement')
   text=json.dumps(data,ensure_ascii=False,separators=(',',':'))+'\n';payload=text.encode('utf-8')
   assert len(payload)<=512*1024,(locale,'pack limit')
   assert len(data['translations'])<=4096,(locale,'entry limit')
   p.write_bytes(payload)
   rows.append({'locale':locale,'bytes':len(payload),'entries':len(data['translations']),'newKeys':len(data['translations'])-len(original['translations'])})
 print(json.dumps({'applied':True,'oldTranslationsPreserved':True,'packs':rows}))
if __name__=='__main__':main()
