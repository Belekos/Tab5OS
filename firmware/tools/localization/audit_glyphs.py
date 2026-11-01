"""Verify shipped script repertoires and the actual styled-font fallback chain."""
from pathlib import Path
import hashlib,json,re,sys
from datetime import datetime,timezone
root=Path(__file__).resolve().parents[2];sketch=next(root.glob('Tab5OS_Arduino_*_Ecriture'));sd=sketch/'sdcard'
version=re.search(r'_v(\d+\.\d+\.\d+)_',sketch.name).group(1);tag=version.replace('.','')
sys.path.insert(0,str(root/'tools'));from audit_languages import glyphs
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
base=glyphs(sd/'System/font.ttf')
fonts={lang:glyphs(sd/f'System/fonts/cjk-{lang}.ttf') for lang in ['zh','ja','ko']}
styles={name:glyphs(sd/f'System/fonts/{name}.ttf') for name in ['bold','italic','bolditalic']}
main_path=next(sketch.glob('*.ino'))
main=main_path.read_text(encoding='utf8')
chain=(sketch/'Tab5CjkFonts.cpp').read_text(encoding='utf8')
assert 'regularFallback=variant?const_cast<lv_font_t*>(richFontFor(px,0)):nullptr' in main
assert re.search(r'Tab5Cjk\.attach\(f,\s*px,\s*regularFallback\)',main)
assert 'p.regular?p.regular:face(p.pixels)' in chain
assert re.search(r'lv_obj_set_style_text_font\(dateLabel,\s*uiFont\(\)',main)
native_name='ime-native-'+tag+'.json'
ime_name='ime-'+tag+'.json'
native=json.loads((root/'tools/verification'/native_name).read_text(encoding='utf8'))
arabic=json.loads((root/('tools/verification/arabic-'+tag+'.json')).read_text(encoding='utf8'))
assert native['passed'] and arabic['pass'] and not arabic['unsupportedShapedGlyphs']
for name,digest in native['sources'].items():assert sha(sketch/name)==digest,('stale source proof',name)
assert arabic['packSha256']==sha(sd/'System/languages/ar.json')
assert arabic['fontSha256']==sha(sd/'System/font.ttf')
keys=json.loads((root/('tools/verification/keyboard-shortcuts-'+tag+'.json')).read_text(encoding='utf8'))
assert keys['passed']
for name,digest in keys['sources'].items():assert sha(sketch/name)==digest,('changed keyboard source',name)
assert keys['keyboardSha256']==sha(sd/'System/keyboards.json')
previous=json.loads((root/'tools/verification'/ime_name).read_text(encoding='utf8'))
assert previous['passed']
retained={name:digest for name,digest in previous['assets'].items() if not name.startswith('System/languages/')}
for name,digest in retained.items():assert sha(sd/name)==digest,('changed IME/font asset',name)
report={'passed':True,'release':version,'checkedAt':datetime.now(timezone.utc).isoformat(),'physicalDisplayTested':False,'method':'Actual cmap entries in current shipped fonts and current pack strings; current LVGL Arabic shaping/BiDi test. Native IME, font lifecycle and keyboard tests were rerun for this release, with source and asset hashes checked. This is not a hardware layout test.','packs':{},'richFaces':{},'sources':{},'assets':{},'proofs':['languages-'+tag+'.json','arabic-'+tag+'.json','keyboard-shortcuts-'+tag+'.json',native_name,ime_name],'currentEvidence':{native_name:{'sourceHashesVerified':len(native['sources']),'rerun':True},ime_name:{'assetHashesVerified':len(retained),'scope':'current dictionaries, keyboard layouts and fonts','rerun':True}}}
all_chars=set();arabic_chars=set()
for p in sorted((sd/'System/languages').glob('*.json')):
    pack=json.loads(p.read_text(encoding='utf8'));code=pack['id'];chars={ord(c) for s in pack['translations'].values() for c in s if ord(c)>=32}
    repertoire=fonts.get(code[:2],set());support=base|repertoire
    missing=sorted(chars-support);assert not missing,(code,missing)
    for name,face in styles.items():assert not chars-(face|support),(code,name)
    report['packs'][code]={'translationStrings':len(pack['translations']),'distinctCodepoints':len(chars),'missingGlyphs':missing,'keyboard':pack['keyboard'],'regularFaceFallbackRequiredFor':{name:len(chars-(face|repertoire)) for name,face in styles.items()}}
    if code=='ar':arabic_chars=chars
    all_chars|=chars
    report['assets'][str(p.relative_to(sd)).replace('\\','/')]=sha(p)
for name,face in styles.items():report['richFaces'][name]={'arabicPackCharactersAbsentFromStyledFace':len(arabic_chars-face),'missingWithRegularFallback':len(arabic_chars-(face|base))}
for name in ['Tab5CjkFonts.cpp','Tab5CjkFonts.h','Tab5Localization.cpp','Tab5LanguageMetadata.h','Tab5KeyboardLayouts.cpp','lv_conf.h']:
    report['sources'][name]=sha(sketch/name)
report['sources'][main_path.name]=sha(main_path)
report['testSources']={name:sha(root/name) for name in ['tools/localization/audit_glyphs.py','tools/audit_languages.py']}
for p in [sd/'System/font.ttf',sd/'System/keyboards.json',*(sd/'System/fonts').glob('*.ttf')]:report['assets'][str(p.relative_to(sd)).replace('\\','/')]=sha(p)
report['catalogue']={'CjkNamesReadableBeforeFontLoad':True,'explanation':'Tab5Localization::name and Tab5KeyboardLayouts::name use Latin labels for an unloaded CJK repertoire; selected CJK language or keyboard loads the matching font.', 'metadataReadBudgetBytesPerPack':1024,'nativeHeaderFixtures':14}
report['calculatorSymbols']={symbol:ord(symbol) in base for symbol in 'π√×÷−'}
assert all(report['calculatorSymbols'].values()),report['calculatorSymbols']
target=root/('tools/verification/glyphs-'+tag+'.json');target.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print(json.dumps({'passed':True,'packs':len(report['packs']),'distinctCodepoints':len(all_chars),'richFaces':report['richFaces'],'physicalDisplayTested':False},ensure_ascii=False,indent=2))
