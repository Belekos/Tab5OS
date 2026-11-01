"""Audit the delivered Japanese pack; does not modify language resources."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re

p=argparse.ArgumentParser()
p.add_argument('--release',type=Path,default=Path(__file__).resolve().parents[2])
a=p.parse_args()
release=a.release.resolve()
sketch=next(release.glob('Tab5OS_Arduino_v*_Ecriture'))
languages=sketch/'sdcard/System/languages'
source=languages/'en-US.json'; target=languages/'ja-JP.json'
en=json.loads(source.read_text(encoding='utf-8'))['translations']
document=json.loads(target.read_text(encoding='utf-8'));ja=document['translations']
fmt=re.compile(r'%(?:[0-9]+\$)?[-+#0 ]*(?:[0-9]+|\*)?(?:\.(?:[0-9]+|\*))?(?:hh|ll|[hljztL])?[diuoxXfFeEgGaAcspn%]')
japanese=re.compile(r'[\u3040-\u30ff\u3400-\u9fff]')
technical={
 'WI-FI','OK','NTP','Ctrl+Shift+C','Ctrl+Shift+I','Ctrl+Shift+S','KiB','MiB',
 'UTC · GMT+0','Wi-Fi','Tab5OS','i','A4\nLetter\nA5','150 dpi\n300 dpi\n600 dpi',
 ' dpi',' px · ',' × ','A','B','C+','C−','H','I','IPP 0x','JPG','L+','L−','×','÷',
 ' · ','HTTP ',' %','z%u | %u x %u px','HTTP 426 · Upgrade: ','\nSHA-256: ',
 ' · z',' · ~',' · #',' · HTTP ',' · TLS ','TLS ','HTTP Upgrade · ','GiB',
 '/join #channel · /msg nickname text · /me text · /nick nickname · /part · /close · /quit',
 'Python','Ctrl+Enter','Ctrl+Shift+O','Ctrl+Shift+R','Ctrl+Shift+L','SELECT','START',
 'RTL-SDR','microSD：%s（%llu MiB）\nUSB：%s（%llu MiB）','microSD · %llu MiB',
 'USB · %s · %llu MiB','USB','Radio Browser'
}
missing=sorted(set(en)-set(ja));extra=sorted(set(ja)-set(en))
formats=[];lines=[];untranslated=[];empty=[];unreviewed=[];help_count=0
for key,english in en.items():
 if key not in ja:continue
 text=ja[key]
 if not isinstance(text,str) or not text:empty.append(key);continue
 if fmt.findall(english)!=fmt.findall(text):formats.append(key)
 if english.count('\n')!=text.count('\n'):lines.append(key)
 if len(english)>=200:
  help_count+=1
  if text==english or text==key or not japanese.search(text):untranslated.append(key)
 if not japanese.search(text) and text not in technical:unreviewed.append(key)
# Keep executable examples and file/URI identifiers intact in the help.
examples=['for f in *.md; do wc "$f"; done','cd /Documents; ls -l; cat "notes.md"; cp "notes.md" "copy.md"',
 'for file in /Documents/*.txt; do echo "$file"; wc "$file"; done',
 'for f in /Documents/*.txt; do echo "$f"; done','find /Documents -name \'*.md\'',
 '/System/keyboards.json','/System/printer-ca.pem','/Pictures/Maps/map-NNNN.jpg',
 '/System/languages/*.json','/System/irc.json','/Saves/GB','/Saves/GBC',
 'sd:/Folder/file.ext','usb:/Folder/file.ext','myaccount.google.com/apppasswords',
 '[profile](Notes.md#characters)','[[Notes#Characters]]','[[Characters|the profile]]']
code_errors=[]
for key,english in en.items():
 if key not in ja:continue
 for example in examples:
  if example in english and example not in ja[key]:code_errors.append({'key':key,'example':example})
metadata=document.get('version')==2 and document.get('id')=='ja-JP' and document.get('name')=='日本語' and document.get('keyboard')=='ja-JP'
report={
 'generated':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'reference':'en-US.json','referenceKeys':len(en),'translatedKeys':len(ja),
 'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'bytes':target.stat().st_size,
 'metadataValid':metadata,'within512KiB':target.stat().st_size<=512*1024,
 'missingKeys':missing,'extraKeys':extra,'emptyValues':empty,
 'printfMismatches':formats,'newlineCountMismatches':lines,
 'longHelpEntriesChecked':help_count,'untranslatedLongHelp':untranslated,
 'unreviewedNonJapaneseValues':unreviewed,'codeExampleMismatches':code_errors,
 'unchangedTechnicalValues':sum(not japanese.search(text) for text in ja.values()),
 'note':'All messages and help translated. Latin-only entries are reviewed units, protocol names, shortcuts, command-only syntax or technical formatting. This structural audit does not replace a Japanese-language visual review on hardware.'
}
report['passed']=metadata and report['within512KiB'] and not any([missing,extra,empty,formats,lines,untranslated,unreviewed,code_errors])
output=release/'tools/verification/ja-JP-0818.json'
output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
raise SystemExit(0 if report['passed'] else 1)
