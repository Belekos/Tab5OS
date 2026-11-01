"""Splice only changed F1 paragraphs, preserving existing translations exactly."""
from pathlib import Path
import hashlib,json,re
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
SKETCH=next(ROOT.glob('Tab5OS_Arduino_*_Ecriture'))
SOURCE=json.loads((HERE/'help-segments-0824.json').read_text(encoding='utf-8'))
SEGMENTS={}
for name in ['help-west-0824.json','help-east-0824.json']:
    block=json.loads((HERE/name).read_text(encoding='utf-8'))
    assert not SEGMENTS.keys()&block.keys()
    SEGMENTS.update(block)
assert len(SEGMENTS)==14
for locale,values in SEGMENTS.items():assert set(values)==set(SOURCE) and all(values.values()),locale
path=SKETCH/'Tab5Help.cpp';text=path.read_text(encoding='utf-8')
pattern=re.compile(r'\{\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*\}')
titles=['Fichiers et sauvegardes','Audio et vidéo','Cartes et lecture','Thèmes, applications et jeux']
pack_paths={p.stem:p for p in (SKETCH/'sdcard/System/languages').glob('*.json')}
packs={locale:json.loads(p.read_text(encoding='utf-8')) for locale,p in pack_paths.items()}
before=HERE/'help-before-0824.json'
if before.exists():original=json.loads(before.read_text(encoding='utf-8'))
else:
    original={}
    for match in pattern.finditer(text):
        title,body=map(json.loads,match.groups())
        if title in titles:original[title]={'source':body,'translations':{locale:p['translations'][body] for locale,p in packs.items()}}
    assert set(original)==set(titles)
    before.write_text(json.dumps(original,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

def splice(title,body,segments):
    paragraphs=body.split('\n\n')
    if title==titles[0]:
        assert len(paragraphs)==6
        return '\n\n'.join([segments['files_first'],*paragraphs[1:],segments['files_menus']])
    if title==titles[1]:
        assert len(paragraphs)==5
        return '\n\n'.join([*paragraphs[:4],segments['media_favorites'],segments['media_resume']])
    if title==titles[2]:return body+'\n\n'+segments['map_resume']
    if title==titles[3]:return body+'\n\n'+segments['gamepad']
    raise ValueError(title)

changed={title:splice(title,row['source'],SOURCE) for title,row in original.items()}
delta={locale:{} for locale in packs};preserved={}
for title,row in original.items():
    fresh=changed[title]
    for locale,pack in packs.items():
        old=row['translations'][locale];new=splice(title,old,SEGMENTS[locale])
        before_parts,after_parts=old.split('\n\n'),new.split('\n\n')
        if title==titles[0]:assert before_parts[1:]==after_parts[1:-1]
        elif title==titles[1]:assert before_parts[:4]==after_parts[:4]
        else:assert new.startswith(old+'\n\n')
        pack['translations'][fresh]=new;delta[locale][fresh]=new
    preserved[title]=len(row['source'].split('\n\n'))-(1 if title in titles[:2] else 0)
sdr_title=SOURCE['sdr_title'];sdr_body='\n\n'.join(SOURCE[k] for k in ['sdr_tuning','sdr_axes','sdr_scan'])
for locale,pack in packs.items():
    for key,value in [(sdr_title,SEGMENTS[locale]['sdr_title']),(sdr_body,'\n\n'.join(SEGMENTS[locale][k] for k in ['sdr_tuning','sdr_axes','sdr_scan']))]:
        pack['translations'][key]=value;delta[locale][key]=value
def replace(match):
    title,body=map(json.loads,match.groups())
    if title not in changed:return match.group()
    return '{'+json.dumps(title,ensure_ascii=False)+', '+json.dumps(changed[title],ensure_ascii=False)+'}'
text=pattern.sub(replace,text)
if not any(json.loads(m.group(1))==sdr_title for m in pattern.finditer(text)):
    marker='\n};\n}';assert text.count(marker)==1
    row='\n  {'+json.dumps(sdr_title,ensure_ascii=False)+', '+json.dumps(sdr_body,ensure_ascii=False)+'},'
    text=text.replace(marker,row+marker)
path.write_text(text,encoding='utf-8')
for locale,pack in packs.items():pack_paths[locale].write_text(json.dumps(pack,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
(HERE/'delta-help-0824.json').write_text(json.dumps(delta,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
report={'release':'0.8.24','passed':True,'changedPages':titles,'addedPage':sdr_title,'changedSegments':len(SOURCE),'translatedLanguages':len(packs),'newDictionaryKeys':6,'unchangedParagraphsPreservedExactlyPerLanguage':preserved,'sourceSha256':hashlib.sha256(path.read_bytes()).hexdigest(),'snapshotSha256':hashlib.sha256(before.read_bytes()).hexdigest()}
(ROOT/'tools/verification/help-0824.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('Updated 4 help pages and added SDR; 10 changed segments in 14 languages; unaffected paragraphs preserved')
