#!/usr/bin/env python3
"""Build editable SD IME dictionaries and optional CJK font subsets.

FontTools is required for --fonts. Sources are fetched separately and checked
against the hashes recorded in cjk-assets-0818.json. No network is used here.
"""
import argparse, collections, copy, gzip, hashlib, json, re, struct
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SKETCH=ROOT/'Tab5OS_Arduino_v0.8.18_Ecriture'
SD=SKETCH/'sdcard/System'

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

def layouts():
    path=SD/'keyboards.json';doc=json.loads(path.read_text(encoding='utf8'))
    existing={x['id']:x for x in doc['keyboards']};base=existing['en-US']
    # The distributed Latin face has no capital sharp S; use the standard
    # German uppercase spelling instead of producing an invisible character.
    for row in existing['de-DE']['upper']:
        for i,key in enumerate(row):
            if key=='ẞ':row[i]='SS'
    latin=list('abcdefghijklmnopqrstuvwxyz');hid={c:i+4 for i,c in enumerate(latin)}
    punctuation={'1':30,'2':31,'3':32,'4':33,'5':34,'6':35,'7':36,'8':37,'9':38,'0':39,'-':45,'=':46,'[':47,']':48,'\\':49,';':51,"'":52,'`':53,',':54,'.':55,'/':56,' ':44}
    hid.update(punctuation)
    def latin_layout(code,name):
        x=copy.deepcopy(base);x['id']=code;x['name']=name
        # Remove unrelated French accent popovers from IME keys.
        for layer in ['lower','upper']:
            x[layer]=[[key.get('value',key) if isinstance(key,dict) and 'value' in key else key for key in row] for row in x[layer]]
        x['hid']=[[i+4,c,c.upper(),c,True,0] for i,c in enumerate(latin)]
        return x
    for code,name in [('id-ID','Bahasa Indonesia (QWERTY)'),('zh-CN','中文（拼音）'),('ja-JP','日本語（ローマ字）'),('ko-KR','한국어 (두벌식)')]:existing[code]=latin_layout(code,name)
    ru=dict(zip('qwertyuiop[]asdfghjkl;\'zxcvbnm,./`','йцукенгшщзхъфывапролджэячсмитьбю.ё'))
    ar=dict(zip('qwertyuiop[]asdfghjkl;\'zxcvbnm,./','ضصثقفغعهخحجدشسيبلاتنمكطئءؤرلاىةوزظ'))
    # Lam-alef is one keyboard key containing two Unicode code points.
    ar.update({'b':'لا','n':'ى','m':'ة',',':'و','.':'ز','/':'ظ'})
    arshift={'q':'َ','w':'ً','e':'ُ','r':'ٌ','t':'لإ','y':'إ','u':'‘','i':'÷','o':'×','p':'؛','[':'<',']':'>','a':'ِ','s':'ٍ','d':']','f':'[','g':'لأ','h':'أ','j':'ـ','k':'،','l':'/',';':':',"'":'"','z':'~','x':'ْ','c':'}','v':'{','b':'لآ','n':'آ','m':'’',',':',','.':'.','/':'؟'}
    us_shift=dict(zip('1234567890-=[]\\;\'`,./','!@#$%^&*()_+{}|:"~<>?'))
    ru_shift=dict(zip('1234567890','!"№;%:?*()'));ru_shift.update({'/':',','-':'_','=':'+'})
    for code,name,mapping in [('ru-RU','Русский (ЙЦУКЕН)',ru),('ar','العربية (١٠١)',ar)]:
        x=latin_layout(code,name)
        # Native rows include punctuation keys that carry actual letters.
        rows=['qwertyuiop[]','asdfghjkl;\'','zxcvbnm,./']
        for upper,layer in [(False,'lower'),(True,'upper')]:
            rendered=[]
            for n,row in enumerate(rows):
                keys=[(arshift.get(c,mapping.get(c,c)) if code=='ar' else mapping.get(c,c).upper()) if upper else mapping.get(c,c) for c in row]
                if n==0:keys.append({'action':'backspace','width':2})
                if n==1:keys.append({'action':'enter','width':2})
                if n==2:keys.insert(0,{'action':'shift','width':2})
                rendered.append(keys)
            rendered.append(copy.deepcopy(base[layer][3]));x[layer]=rendered
        x['hid']=[[usage,mapping.get(c,c),(arshift.get(c,us_shift.get(c,mapping.get(c,c))) if code=='ar' else ru_shift.get(c,mapping.get(c,c).upper())),c,code=='ru-RU' and mapping.get(c,c).isalpha(),0] for c,usage in hid.items()]
        if code=='ru-RU':
            for layer,a,b in [('lower','е','ё'),('upper','Е','Ё')]:
                for row in x[layer]:
                    for i,key in enumerate(row):
                        if key==a:row[i]={'value':a,'accents':[a,b]}
        existing[code]=x
    ko=dict(zip('qwertyuiopasdfghjklzxcvbnm','ㅂㅈㄷㄱㅅㅛㅕㅑㅐㅔㅁㄴㅇㄹㅎㅗㅓㅏㅣㅋㅌㅊㅍㅠㅜㅡ'))
    shift={'q':'ㅃ','w':'ㅉ','e':'ㄸ','r':'ㄲ','t':'ㅆ','o':'ㅒ','p':'ㅖ'}
    for layer in ['lower','upper']:
        for row in existing['ko-KR'][layer]:
            for i,key in enumerate(row):
                if isinstance(key,str) and key.lower() in ko:
                    native=shift.get(key.lower(),ko[key.lower()]) if layer=='upper' else ko[key]
                    row[i]={'value':key,'label':native+' '+key}
    for code in ['zh-CN','ja-JP','ko-KR']:existing[code]['input_method']={'zh-CN':'pinyin','ja-JP':'romaji','ko-KR':'2-beolsik'}[code]
    doc['keyboards']=list(existing.values());doc['version']=3
    path.write_text(json.dumps(doc,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    return {'layouts':len(existing),'ids':list(existing)}

def fonts(source):
    from fontTools.ttLib import TTFont
    from fontTools.varLib.instancer import instantiateVariableFont
    from fontTools import subset
    result={}
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
    p=argparse.ArgumentParser();p.add_argument('--sources',type=Path,required=True);p.add_argument('--fonts',action='store_true');args=p.parse_args()
    report={'layouts':layouts(),'dictionaries':dictionaries(args.sources)}
    if args.fonts:report['fonts']=fonts(args.sources)
    report['sources']={x.name:{'sha256':hashlib.sha256(x.read_bytes()).hexdigest(),'bytes':x.stat().st_size} for x in args.sources.iterdir() if x.suffix in ['.ttf','.yaml'] or x.name=='SKK-JISYO.L'}
    dest=ROOT/'tools/verification/cjk-assets-0818.json';dest.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8');print(json.dumps(report,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
