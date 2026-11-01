"""Apply reviewed .51 Drawing labels; remove one unreachable superseded help key.

No runtime memory limit, user file, language metadata or active help is removed.
The delivered .50 ZIP is the authoritative source for preservation comparisons.
"""
from pathlib import Path
import argparse, hashlib, json, sys, zipfile
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from release_config import SKETCH
BASELINE_SHA256='ea13e38382767ee6e061d5a099b553f6a42461454b1a5bbd7bc653893a802c3b'
PREFIX='Tab5OS_Ecriture_v0.8.50/Tab5OS_Arduino_v0.8.50_Ecriture/'
OLD_PREFIX='Dans Explorateur, Haut/Bas sélectionnent, Entrée ouvre et Retour arrière remonte au dossier parent. Un clic sélectionne ; un double-clic ouvre. Au toucher, le premier appui sélectionne et un'
GENERATED={'Tab5EnglishFallback.h','Tab5EnglishBitmap.h','Tab5SystemResourcesData.h'}

def obsolete_key(rows):
    keys=[k for k in rows if k.startswith(OLD_PREFIX)]
    assert len(keys)==1, 'Unexpected obsolete help match'
    return keys[0]

def checked_live_sources(sketch):
    paths=[]
    for path in sketch.rglob('*'):
        if not path.is_file() or path.name in GENERATED or any(p in path.parts for p in ('Factory','test-output','test')):
            continue
        if path.suffix not in ('.c','.cpp','.h','.ino','.py','.elf'):
            continue
        assert OLD_PREFIX.encode() not in path.read_bytes(), ('Obsolete help still referenced',str(path))
        paths.append(path)
    assert len(paths)>1900
    return paths

def deltas():
    result={}
    for path in sorted(HERE.glob('relief-*-delta-0851.json')):
        for lang,rows in json.loads(path.read_text()).items():
            assert lang not in result, ('Duplicate locale delta',lang)
            result[lang]=rows
    assert len(result)==14
    keys=[set(v) for v in result.values()]
    assert all(k==keys[0] for k in keys)
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',type=Path,required=True)
    a=parser.parse_args()
    assert hashlib.sha256(a.baseline.read_bytes()).hexdigest()==BASELINE_SHA256
    sketch=ROOT/SKETCH; sources=checked_live_sources(sketch); delta=deltas(); report={}
    with zipfile.ZipFile(a.baseline) as archive:
        for path in sorted((sketch/'sdcard/System/languages').glob('*.json')):
            original=json.loads(archive.read(PREFIX+'sdcard/System/languages/'+path.name))
            pack=json.loads(path.read_text()); old=obsolete_key(original['translations'])
            assert {k:v for k,v in pack.items() if k!='translations'}=={k:v for k,v in original.items() if k!='translations'}
            rows=pack['translations']; rows.pop(old,None)
            for key,value in delta[path.stem].items(): rows[key]=value
            assert all(rows.get(k)==v for k,v in original['translations'].items() if k not in (old,'Relief'))
            assert set(rows)-set(original['translations'])==set(delta[path.stem])-set(original['translations'])
            encoded=json.dumps(pack,ensure_ascii=False,separators=(',',':'))+'\n'
            assert len(encoded.encode())<=512*1024,(path.name,'runtime size limit')
            assert len(rows)<=4096
            path.write_text(encoded)
            report[path.stem]={'beforeBytes':len(archive.read(PREFIX+'sdcard/System/languages/'+path.name)),'afterBytes':len(encoded.encode()),'removedKeySha256':hashlib.sha256(old.encode()).hexdigest(),'removedSourceBytes':len(old.encode()),'removedTranslationBytes':len(original['translations'][old].encode()),'entries':len(rows)}
    out=ROOT/'tools/verification/localization-migration-0851.json'
    out.write_text(json.dumps({'version':'0.8.51','passed':True,'baselineZipSha256':BASELINE_SHA256,'obsoleteKeyPrefix':OLD_PREFIX,'unusedReferenceScanFiles':len(sources),'removedKeysPerPack':1,'changedExistingKeys':['Relief'],'packs':report},ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'packs':len(report),'liveSourceFilesScanned':len(sources),'largestPack':max((r['afterBytes'],lang) for lang,r in report.items())}))

if __name__=='__main__':main()
