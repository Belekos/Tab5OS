from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from test_report_0842 import save_report
import argparse,hashlib,json,shutil,subprocess,zipfile,os
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',type=Path);a=p.parse_args();here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.42_Ecriture';build=release.parent.parent/'build_0842/persistence-host';build.mkdir(parents=True,exist_ok=True)
def block(text,signature):
 start=text.index(signature)
 while 0<=text.find(';',start)<text.index('{',start):start=text.index(signature,start+len(signature))
 at=text.index('{',start)+1;depth=1
 while depth:
  if text[at]=='{':depth+=1
  elif text[at]=='}':depth-=1
  at+=1
 return text[start:at]
def source(name):
 if a.baseline_zip:
  with zipfile.ZipFile(a.baseline_zip) as z:
   paths=[p for p in z.namelist() if (p.endswith('/'+name) or name=='.ino' and p.endswith('.ino')) and '/tools/' not in p];assert len(paths)==1;return z.read(paths[0]).decode('utf-8')
 return (sketch/(sketch.name+'.ino' if name=='.ino' else name)).read_text()
text=source('.ino');helper=source('Tab5AtomicFile.h');(build/'Tab5AtomicFile.h').write_text(helper)
for name in ['Arduino.h','FS.h']:shutil.copy2(here/name,build/name)
for name in ['Tab5VolumeFS.h','Tab5Storage.h','SD.h']:(build/name).write_text('#include "FS.h"\n')
for name in ['Tab5SystemMigration.cpp','Tab5SystemMigration.h']:(build/name).write_text(source(name))
results={};hashes={}
for test in ['settings','progress']:
 signatures=['static bool migrateSystemDirectory()', 'static bool saveSettings()', 'static void loadSettings()', 'static bool loadSettingsSources()', 'static bool reloadSettingsAfterMigration()'] if test=='settings' else ['static uint32_t readerProgressHash(', 'static String readerProgressPath(const String& path) {', 'static bool loadReaderProgress(', 'static void migrateReaderProgress(', 'static void saveReaderProgressNow() {', 'static bool atomicWriteText(']
 chunks=[block(text,s) for s in signatures];hashes.update({n:hashlib.sha256(c.encode()).hexdigest() for n,c in zip(signatures,chunks)});testSource=(here/(test+'.cpp')).read_text().replace('/* CONTROLLERS */','\n'.join(chunks)).replace('/* SETTINGS STRUCT */',block(text,'struct AppSettings')+';');(build/(test+'.cpp')).write_text(testSource)
 cmd=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(build),str(build/(test+'.cpp'))]+([str(build/'Tab5SystemMigration.cpp')] if test=='settings' else [])+['-o',str(build/test)]
 r=subprocess.run(cmd,capture_output=True,text=True);print(r.stdout+r.stderr,end='');r.check_returncode();r=subprocess.run([str(build/test)]+(['baseline'] if a.baseline_zip else []),capture_output=True,text=True,timeout=30,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});print(r.stdout+r.stderr,end='');r.check_returncode();results[test]=r.stdout.strip()
report=dict(sourceHashes={str(sketch.name+'/'+sketch.name+'.ino'):hashlib.sha256(text.encode()).hexdigest(),str(sketch.name+'/Tab5AtomicFile.h'):hashlib.sha256(helper.encode()).hexdigest()},version='0.8.42',baseline=bool(a.baseline_zip),physicalDeviceTested=False,controllers=hashes,atomicHelperSha256=hashlib.sha256(helper.encode()).hexdigest(),results=results,method='Actual main-sketch settings/document/progress functions extracted verbatim, actual AtomicFile and SystemMigration, fault-injected filesystem, AddressSanitizer/UBSan. LeakSanitizer disabled: this container cannot enumerate /proc threads. No hardware durability or measured-leak claim.')
save_report(report,(release/'tools/verification'/('persistence-0842-baseline.json' if a.baseline_zip else 'persistence-0842.json')),here,'\n'.join(results.values()))
