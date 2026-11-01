"""Check the actual .20 translations and record their current hashes."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,sys
root=Path(__file__).resolve().parents[2];sketch=next(root.glob('Tab5OS_Arduino_*_Ecriture'));here=Path(__file__).resolve().parent
sys.path.insert(0,str(root/'tools'));from audit_languages import signature
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
keys=json.loads((here/'reference-delta-0820.json').read_text(encoding='utf8'))
delta=json.loads((here/'delta-ui-0820.json').read_text(encoding='utf8'));assert len(delta)==14
report={'passed':True,'release':'0.8.20','checkedAt':datetime.now(timezone.utc).isoformat(),'newSourceKeys':len(keys),'deltaFiles':{'delta-ui-0820.json':sha(here/'delta-ui-0820.json')},'packs':{}}
for p in sorted((sketch/'sdcard/System/languages').glob('*.json')):
    pack=json.loads(p.read_text(encoding='utf8'));code=pack['id'];rows=pack['translations']
    assert set(delta[code])==set(keys)
    for key in keys:
        assert rows[key]==delta[code][key] and rows[key],(code,key,'mismatch')
        assert signature(key)==signature(rows[key]),(code,key,'format')
        assert key.count('\n')==rows[key].count('\n'),(code,key,'newlines')
    report['packs'][code]={'translatedNewKeys':len(keys),'packEntries':len(rows),'sha256':sha(p)}
assert len(report['packs'])==14
(root/'tools/verification/delta-languages-0820.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('PASS current .20 delta:',len(keys),'keys in',len(report['packs']),'packs.')
