from pathlib import Path
import json,hashlib,zipfile,argparse,sys
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SKETCH=ROOT/'Tab5OS_Arduino_v0.8.34_Ecriture'
sys.path.insert(0,str(ROOT/'tools'))
from audit_languages import signature

def read(p): return json.loads(p.read_text(encoding='utf-8'))
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--baseline',type=Path,required=True);args=parser.parse_args()
 baseline_sha=hashlib.sha256(args.baseline.read_bytes()).hexdigest()
 assert baseline_sha=='c2716546724868064a0fa22808efc425d15b0ff055a92e32efb50fa965b9d8d6'
 pieces=[read(HERE/'drawing-ui-delta-0834.json'),read(HERE/'drawing-diagnostics-delta-0834.json')]
 locales=sorted(pieces[0]); delta={locale:{} for locale in locales}
 for part in pieces:
  assert sorted(part)==locales
  for locale,rows in part.items():
   for k,v in rows.items():
    assert k not in delta[locale] or delta[locale][k]==v
    assert isinstance(v,str) and v
    assert signature(k)==signature(v),(locale,k,'format')
    assert k.count('\n')==v.count('\n'),(locale,k,'newlines')
    delta[locale][k]=v
 keys=set(delta['fr-FR'])
 assert all(set(rows)==keys for rows in delta.values())
 report={'version':'0.8.34','passed':True,'baselineZipSha256':baseline_sha,'newKeys':len(keys),'baselineTranslationsAndMetadataPreserved':True,'packs':{}}
 with zipfile.ZipFile(args.baseline) as z:
  for locale,rows in delta.items():
   rel='sdcard/System/languages/'+locale+'.json'
   old=z.read('Tab5OS_Ecriture_v0.8.33/Tab5OS_Arduino_v0.8.33_Ecriture/'+rel)
   orig=json.loads(old);path=SKETCH/rel;pack=read(path)
   assert {k:v for k,v in pack.items() if k!='translations'}=={k:v for k,v in orig.items() if k!='translations'}
   assert all(pack['translations'].get(k)==v for k,v in orig['translations'].items()),(locale,'existing translation changed')
   for k,v in rows.items():
    assert k not in orig['translations'] or orig['translations'][k]==v,(locale,k,'old key conflict')
    pack['translations'][k]=v
   data=(json.dumps(pack,ensure_ascii=False,indent=2)+'\n').encode('utf-8')
   assert len(data)<=512*1024,(locale,len(data),'size limit')
   path.write_bytes(data)
   names={k:len(rows[k].encode()) for k in ('Image BMP','Image JPEG')}
   assert max(names.values())<=23
   report['packs'][locale]={'newKeys':len(set(rows)-set(orig['translations'])),'entries':len(pack['translations']),'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'layerNameBytes':names}
 (HERE/'drawing-delta-0834.json').write_text(json.dumps(delta,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 (ROOT/'tools/verification/drawing-localization-0834.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print('PASS',len(keys),'keys;',len(locales),'packs; Russian',report['packs']['ru-RU']['bytes'],'bytes')
if __name__=='__main__': main()
