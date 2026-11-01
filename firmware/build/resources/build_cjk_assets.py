#!/usr/bin/env python3
"""Build editable SD IME dictionaries and optional CJK font subsets.

FontTools is required for --fonts. Sources are supplied separately; their
hashes are recorded in the output report. No network is used here.
"""
import argparse, collections, gzip, hashlib, json, re, struct
from pathlib import Path

SD = None  # Set from the explicitly selected sketch by main().

def write_dictionary(path, groups):
    rows=[]
    for key,values in groups.items():
        values=list(dict.fromkeys(values))
        if not key or len(key.encode())>96: continue
        values=[x for x in values if x and len(x.encode())<=127 and '\0' not in x]
        if values: rows.append((key,values))
    rows.sort(key=lambda x:x[0].encode())
    base=16+4*(len(rows)+1); offsets=[]; data=bytearray(); candidates=0
    for key,values in rows:
        offsets.append(base+len(data)); data+=key.encode()+b'\0'
        table=base+len(data); start=table+4+4*(len(values)+1)
        words=bytearray(); positions=[]
        for value in values: positions.append(start+len(words));words+=value.encode()+b'\0'
        positions.append(start+len(words))
        data+=struct.pack('<I',len(values))+struct.pack('<'+'I'*len(positions),*positions)+words
        candidates+=len(values)
    offsets.append(base+len(data));path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes(b'T5IME01\0'+struct.pack('<II',len(rows),base)+struct.pack('<'+'I'*len(offsets),*offsets)+data)
    return {'keys':len(rows),'candidates':candidates,'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}

def dictionaries(source):
    out=SD/'ime';out.mkdir(parents=True,exist_ok=True);lexicons={}
    zh=collections.defaultdict(list)
    for line in (source/'pinyin_simp.dict.yaml').read_text(encoding='utf8').splitlines():
        fields=line.split('\t')
        if len(fields)<2:continue
        word,key=fields[:2];key=key.replace(' ','').replace('ü','v').lower()
        if not re.fullmatch('[a-z]+',key):continue
        try: weight=float(fields[2].strip('%')) if len(fields)>2 else 0
        except ValueError:weight=0
        zh[key].append((weight,word))
    zh={key:[word for _,word in sorted(words,reverse=True)] for key,words in zh.items()}
    ja=collections.defaultdict(list);okuri=False; skipped=0
    for line in (source/'SKK-JISYO.L').read_text(encoding='euc_jp').splitlines():
        if line.startswith(';; okuri-ari'):okuri=True
        if line.startswith(';; okuri-nasi'):okuri=False
        if line.startswith(';') or not line.strip() or ' /' not in line:continue
        key,values=line.split(' /',1)
        if not all('\u3040'<=x<='\u309f' or x=='ー' for x in key):
            skipped+=1;continue
        # Dictionary expressions and annotations are data, never evaluated.
        for candidate in values.strip('/').split('/'):
            candidate=candidate.split(';',1)[0]
            if candidate and not any(c in candidate for c in '[]()#\\'):
                ja[key].append(candidate)
    for lang,groups in [('zh',zh),('ja',ja)]:
        path=out/('zh-pinyin.dat' if lang=='zh' else 'ja-kana.dat')
        lexicons[lang]=write_dictionary(path,groups)
        (source/(lang+'-characters.txt')).write_text(''.join(sorted(set(''.join(x for values in groups.values() for x in values)))),encoding='utf8')
    lexicons['ja']['excluded_okurigana_abbreviations_and_expressions']=skipped
    for name in ['SKK-JISYO.L','pinyin_simp.dict.yaml']:
        (out/'sources').mkdir(exist_ok=True)
        with gzip.GzipFile(filename=str(out/'sources'/(name+'.gz')),mode='wb',mtime=0) as f:f.write((source/name).read_bytes())
    for name in ['OFL.txt','pinyin-LICENSE.txt','SKK-COPYING.txt']:
        if (source/name).exists():(out/name).write_bytes((source/name).read_bytes())
    return lexicons

def fonts(source):
    from fontTools.ttLib import TTFont
    from fontTools.varLib.instancer import instantiateVariableFont
    from fontTools import subset
    result={}
    (SD/'fonts').mkdir(parents=True, exist_ok=True)
    (SD/'fonts').mkdir(parents=True, exist_ok=True)
    for lang,name in [('zh','SC'),('ja','JP'),('ko','KR')]:
        font=TTFont(source/('NotoSans'+name+'.ttf'))
        font=instantiateVariableFont(font,{'wght':400},inplace=True)
        # No hint interpreter in TinyTTF. Dropping hints/layout tables makes
        # a complete input repertoire practical without shrinking UI text.
        options=subset.Options();options.hinting=False;options.layout_features=[]
        options.drop_tables+=['BASE','GPOS','GSUB','GDEF','VORG','DSIG','vhea','vmtx','VVAR','STAT']
        chars=set(range(0x3000,0x3100))|set(range(0xff00,0xffef))
        required=set()
        if lang=='ko':
            required|=set(range(0xac00,0xd7a4));chars|=required|set(range(0x3130,0x3190))
        else:
            required|=set(map(ord,(source/(lang+'-characters.txt')).read_text(encoding='utf8')));chars|=required
        # Always retain every glyph in the UI language pack.
        for pack in (SD/'languages').glob({'zh':'zh-*.json','ja':'ja-*.json','ko':'ko-*.json'}[lang]):
            j=json.loads(pack.read_text(encoding='utf8'));required|=set(map(ord,''.join(j['translations'].values())));chars|=required
        primary=TTFont(SD/'font.ttf')
        primary_chars=set(primary.getBestCmap());chars-=primary_chars;required-=primary_chars;primary.close()
        available=set(font.getBestCmap());missing=chars-available
        sub=subset.Subsetter(options=options);sub.populate(unicodes=chars);sub.subset(font)
        # Modified font name, as required when distributing a subset.
        for record in font['name'].names:
            if record.nameID in (1,3,4,6):record.string=('Tab5CJK'+name if record.nameID==6 else 'Tab5 CJK '+name).encode('utf-16be' if record.isUnicode() else 'ascii')
        font.recalcTimestamp=False
        font['head'].modified=font['head'].created
        target=SD/'fonts'/('cjk-'+lang+'.ttf');font.save(target)
        result[lang]={'bytes':target.stat().st_size,'glyphs':len(font.getBestCmap()),'excludedUnmappedRangeCodepoints':len(missing),'missingRequiredGlyphs':sorted(cp for cp in required-available if cp>=32),'sha256':hashlib.sha256(target.read_bytes()).hexdigest()}
    return result

def main():
    global SD
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sketch', type=Path, required=True,
                        help='Sketch directory containing sdcard/System')
    parser.add_argument('--sources', type=Path, required=True,
                        help='Directory containing dictionary and optional font inputs')
    parser.add_argument('--fonts', action='store_true')
    parser.add_argument('--report', type=Path,
                        help='Optional JSON report path; defaults to System/ime/build-report.json')
    args = parser.parse_args()
    SD = args.sketch.resolve() / 'sdcard/System'
    if not SD.is_dir():
        parser.error('The sketch must contain sdcard/System')
    report = {'dictionaries': dictionaries(args.sources)}
    if args.fonts:
        report['fonts'] = fonts(args.sources)
    report['sources'] = {
        x.name: {'sha256': hashlib.sha256(x.read_bytes()).hexdigest(), 'bytes': x.stat().st_size}
        for x in args.sources.iterdir()
        if x.suffix in ['.ttf', '.yaml'] or x.name == 'SKK-JISYO.L'
    }
    destination = args.report or SD / 'ime/build-report.json'
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report, ensure_ascii=False, indent=2))

if __name__ == '__main__':
    main()
