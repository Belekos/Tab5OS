"""Verify all dictionaries, keyboard mappings and final font/pack coverage."""
from pathlib import Path
import hashlib,json,struct,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1];SKETCH=ROOT/'Tab5OS_Arduino_v0.8.28_Ecriture';SD=SKETCH/'sdcard'
sys.path.insert(0,str(ROOT/'tools'));from audit_languages import glyphs
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
native=json.loads((ROOT/'tools/verification/ime-native-0828.json').read_text(encoding='utf8'))
assert native['passed']
for p,h in native['sources'].items():assert sha(SKETCH/p)==h,('stale native source',p)
base=glyphs(SD/'System/font.ttf');fonts={x:base|glyphs(SD/f'System/fonts/cjk-{x}.ttf') for x in ['zh','ja','ko']}
problems=[];lexicons={}
for lang,name in [('zh','zh-pinyin.dat'),('ja','ja-kana.dat')]:
    data=(SD/'System/ime'/name).read_bytes();assert data[:8]==b'T5IME01\0';count,start=struct.unpack_from('<II',data,8)
    offsets=struct.unpack_from('<'+'I'*(count+1),data,16);assert start==offsets[0] and offsets[-1]==len(data)
    keys=[];candidate_count=0;characters=set()
    for a,b in zip(offsets,offsets[1:]):
        assert a<b<=len(data);key_end=data.index(0,a,b);key=data[a:key_end].decode('utf8');keys.append(key.encode());table=key_end+1
        n=struct.unpack_from('<I',data,table)[0];positions=struct.unpack_from('<'+'I'*(n+1),data,table+4);assert positions[0]==table+4+4*(n+1) and positions[-1]==b
        for c,d in zip(positions,positions[1:]):
            assert c<d<=b and data[d-1]==0;word=data[c:d-1].decode('utf8');assert 0<len(word.encode())<=127
            characters.update(ord(x) for x in word if ord(x)>=32)
        candidate_count+=n
    assert keys==sorted(set(keys));missing=sorted(characters-fonts[lang]);
    if missing:problems.append((lang,'dictionary missing glyphs',[hex(x) for x in missing]))
    lexicons[lang]={'keys':count,'candidates':candidate_count,'uniqueCharacters':len(characters),'missingGlyphs':missing,'bytes':len(data)}
layout_doc=json.loads((SD/'System/keyboards.json').read_text(encoding='utf8'));layouts={x['id']:x for x in layout_doc['keyboards']}
for code in ['fr-FR','en-US','de-DE','es-ES','it-IT','nl-NL','pt-BR','pt-PT','id-ID','ru-RU','ar','zh-CN','ja-JP','ko-KR']:assert code in layouts
for code,x in layouts.items():
    supported=fonts.get(code[:2],base)
    assert len(x.get('hid',[]))<=128
    used=set()
    for row in x.get('hid',[]):
        assert row[0] not in used;used.add(row[0]);assert all(len(value.encode())<8 for value in row[1:4])
        missing=sorted({ord(c) for value in row[1:4] for c in value if ord(c)>31}-supported)
        if missing:problems.append((code,'physical HID '+str(row[0]),missing))
    for layer in ['lower','upper','symbols']:
        assert 1<=len(x[layer])<=4
        for row in x[layer]:
            assert 1<=len(row)<=14
            for key in row:
                if isinstance(key,str):text=key
                else:text=key.get('label',key.get('value',''))+''.join(key.get('accents',[]))
                if text.startswith('<'):continue
                missing=sorted({ord(c) for c in text if ord(c)>31}-supported)
                if missing:problems.append((code,layer,text,missing))
ru={r[0]:r for r in layouts['ru-RU']['hid']};ar={r[0]:r for r in layouts['ar']['hid']}
assert ru[4][1:3]==['ф','Ф'] and ru[32][2]=='№' and ru[51][4] is True
assert ar[5][1]=='لا' and ar[20][1:3]==['ض','َ']
assert set(range(0xac00,0xd7a4))<=fonts['ko']
packs=list((SD/'System/languages').glob('*.json'))
assert len(packs)==14,('expected complete set of 14 packs',len(packs))
for p in packs:
    pack=json.loads(p.read_text(encoding='utf8'));assert pack.get('keyboard') in layouts
    supported=fonts.get(pack['id'][:2],base)
    missing=sorted({ord(c) for t in pack['translations'].values() for c in t if ord(c)>31}-supported)
    if missing:problems.append((pack['id'],'pack glyphs',missing))
sources={name:sha(SKETCH/name) for name in ['Tab5Ime.h','Tab5ImeDictionary.h','Tab5ImeDictionary.cpp','Tab5ImeUi.inc','Tab5CjkFonts.h','Tab5CjkFonts.cpp','Tab5KeyboardLayouts.h','Tab5KeyboardLayouts.cpp','Tab5Localization.cpp']}
assets={str(p.relative_to(SD)).replace('\\','/'):sha(p) for p in [SD/'System/keyboards.json',SD/'System/font.ttf']+list((SD/'System/ime').rglob('*'))+list((SD/'System/fonts').glob('cjk-*.ttf'))+list((SD/'System/languages').glob('*.json')) if p.is_file()}
report={'version':'0.8.28','passed':not problems,'sources':sources,'assets':assets,'nativeProof':'ime-native-0828.json','dictionaries':lexicons,'layouts':len(layouts),'modernHangulSyllables':11172,'glyphProblems':problems,'physicalDisplayTested':False}
(ROOT/'tools/verification/ime-0828.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print(json.dumps({k:v for k,v in report.items() if k not in ('sources','assets')},ensure_ascii=False,indent=2));raise SystemExit(bool(problems))
