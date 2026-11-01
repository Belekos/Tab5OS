"""Check the reviewed .25 delta against every actual language pack."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,sys
HERE=Path(__file__).resolve().parent;RELEASE=HERE.parents[1];SKETCH=next(RELEASE.glob('Tab5OS_Arduino_*_Ecriture'))
sys.path.insert(0,str(RELEASE/'tools'));from audit_languages import signature
read=lambda path:json.loads(path.read_text(encoding='utf8'))
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
delta=read(HERE/'delta-0825.json');keys=read(HERE/'reference-final-delta-0825.json')
assert len(delta)==14 and set(keys)==set(delta['fr-FR'])
report={'release':'0.8.25','checkedAt':datetime.now(timezone.utc).isoformat(),'newKeys':len(keys),'passed':True,'sources':{},'packs':{}}
for name in [*read(HERE/'delta-inputs-0825.json'),'delta-0825.json','reference-final-delta-0825.json','audit_delta_0825.py','apply_delta_0825.py']:
    report['sources'][name]=sha(HERE/name)
technical={'Correction (ppm)'}
for locale,values in delta.items():
    assert set(values)==set(keys),(locale,'incomplete delta')
    path=SKETCH/'sdcard/System/languages'/(locale+'.json');pack=read(path)['translations'];same=[]
    for key,value in values.items():
        assert isinstance(value,str) and value and pack.get(key)==value,(locale,key,'missing or different')
        assert signature(key)==signature(value),(locale,key,'printf')
        assert key.count('\n')==value.count('\n'),(locale,key,'newlines')
        if locale!='fr-FR' and key==value:
            assert key in technical,(locale,key,'untranslated');same.append(key)
    report['packs'][locale]={'translatedKeys':len(keys),'packEntries':len(pack),'sharedTechnicalTerms':same,'sha256':sha(path)}
(RELEASE/'tools/verification/delta-languages-0825.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('PASS .25 delta:',len(keys),'keys in',len(delta),'language packs')
