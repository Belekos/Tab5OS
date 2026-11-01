"""Apply complete reviewed UI/backend/help dictionaries for .25."""
from pathlib import Path
import hashlib,json,re,zipfile
HERE=Path(__file__).resolve().parent;RELEASE=HERE.parents[1]
SKETCH=next(RELEASE.glob('Tab5OS_Arduino_*_Ecriture'))
LOCALES={'ar','de-DE','en-US','es-ES','fr-FR','id-ID','it-IT','ja-JP','ko-KR','nl-NL','pt-BR','pt-PT','ru-RU','zh-CN'}
ALIAS={'fr':'fr-FR','ru':'ru-RU','zh':'zh-CN','ja':'ja-JP','ko':'ko-KR','id':'id-ID'}
read=lambda name:json.loads((HERE/name).read_text(encoding='utf-8-sig'))
write=lambda path,value:path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
delta={locale:{} for locale in LOCALES};inputs=[]
for group in [('delta-ui-0825.json',),('delta-shell-0825.json',),('delta-indirect-0825.json',),
              ('vault-backend-west-0825.json','vault-backend-east-0825.json'),
              ('vault-ui-west-0825.json','vault-ui-east-0825.json')]:
    rows={}
    for name in group:
        inputs.append(name)
        for locale,values in read(name).items():
            locale=ALIAS.get(locale,locale);assert locale not in rows,(group,locale)
            rows[locale]=values
    if 'fr-FR' not in rows:rows['fr-FR']={key:key for key in rows['en-US']}
    assert set(rows)==LOCALES,group
    for locale,values in rows.items():
        assert set(values)==set(rows['fr-FR']),(group,locale,'incomplete')
        delta[locale].update(values)
packs={locale:json.loads((SKETCH/'sdcard/System/languages'/(locale+'.json')).read_text(encoding='utf8')) for locale in LOCALES}
# These four messages differ solely by the separator from an existing reviewed
# message. Keep the exact existing translation instead of inventing a fallback.
aliases={key:key.replace(' - ',' — ') for key in ['Coffre appareil illisible - ancien coffre conservé','Clés copiées - ancien coffre conservé','Reconnexion SD en cours - réessayez','Copie chiffrée créée - original conservé']}
alias_delta={locale:{key:pack['translations'][old] for key,old in aliases.items()} for locale,pack in packs.items()}
write(HERE/'delta-alias-0825.json',alias_delta);inputs.append('delta-alias-0825.json')
for locale,values in alias_delta.items():delta[locale].update(values)
segments=read('help-west-0825.json');inputs.append('help-west-0825.json')
for locale,values in read('files-help-east-0825.json').items():segments.setdefault(ALIAS.get(locale,locale),{}).update(values)
inputs.append('files-help-east-0825.json')
for locale,value in read('sdr-quality-east-0825.json').items():
    locale=ALIAS.get(locale,locale)
    if locale!='fr-FR':segments.setdefault(locale,{})['sdr_quality']=value
inputs.append('sdr-quality-east-0825.json')
for name in ('vault-help-west-0825.json','vault-help-east-0825.json'):
    inputs.append(name)
    for locale,values in read(name).items():segments.setdefault(ALIAS.get(locale,locale),{}).update(values)
assert set(segments)==LOCALES
for locale,values in segments.items():assert set(values)==set(segments['fr-FR']),(locale,'help segments')
pattern=re.compile(r'\{\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*\}')
titles=['Fichiers et sauvegardes','Radio SDR','Coffre et récupération']
with zipfile.ZipFile(RELEASE.parent/'Tab5OS_0.8.24_corrections.zip') as z:
    base='Tab5OS_Ecriture_v0.8.24/Tab5OS_Arduino_v0.8.24_Ecriture/'
    oldtext=z.read(base+'Tab5Help.cpp').decode('utf8')
    original={json.loads(m[0]):{'source':json.loads(m[1]),'translations':{}} for m in pattern.findall(oldtext) if json.loads(m[0]) in titles}
    for locale in LOCALES:
        translations=json.loads(z.read(base+'sdcard/System/languages/'+locale+'.json'))['translations']
        for row in original.values():row['translations'][locale]=translations[row['source']]
write(HERE/'help-before-0825.json',original)
def splice(title,text,row):
    paras=text.split('\n\n')
    if title==titles[0]:
        assert len(paras)==7
        return '\n\n'.join([row['files_first'],*paras[1:],row['files_script']])
    if title==titles[1]:return text+'\n\n'+row['sdr_quality']
    if title==titles[2]:
        assert len(paras)==4
        return '\n\n'.join([row['vault_access'],paras[1],row['vault_backup'],paras[3],row['vault_secrets']])
    raise ValueError(title)
changed={title:splice(title,row['source'],segments['fr-FR']) for title,row in original.items()}
help_delta={locale:{} for locale in LOCALES}
for title,row in original.items():
    for locale in LOCALES:
        value=splice(title,row['translations'][locale],segments[locale])
        delta[locale][changed[title]]=value;help_delta[locale][changed[title]]=value
write(HERE/'delta-help-0825.json',help_delta);inputs.append('delta-help-0825.json')
path=SKETCH/'Tab5Help.cpp';text=path.read_text(encoding='utf8')
def replace(match):
    title=json.loads(match[1])
    return '{'+json.dumps(title,ensure_ascii=False)+', '+json.dumps(changed[title],ensure_ascii=False)+'}' if title in changed else match.group()
path.write_text(pattern.sub(replace,text),encoding='utf8')
for locale,pack in packs.items():
    pack['translations'].update(delta[locale]);write(SKETCH/'sdcard/System/languages'/(locale+'.json'),pack)
write(HERE/'delta-0825.json',delta)
write(HERE/'reference-final-delta-0825.json',list(delta['fr-FR']))
write(HERE/'delta-inputs-0825.json',inputs)
write(RELEASE/'tools/verification/help-0825.json',{'version':'0.8.25','passed':True,'physicalDeviceTested':False,
 'changedPages':titles,'translatedLanguages':14,'newDictionaryKeys':3,'unchangedParagraphsPreservedExactlyPerLanguage':dict(zip(titles,[6,3,2])),
 'sourceSha256':hashlib.sha256(path.read_bytes()).hexdigest(),'snapshotSha256':hashlib.sha256((HERE/'help-before-0825.json').read_bytes()).hexdigest(),
 'sources':{name:hashlib.sha256((HERE/name).read_bytes()).hexdigest() for name in inputs if 'help' in name or 'quality' in name}})
print('Applied',len(delta['fr-FR']),'reviewed keys in14languages; updated3help pages')
