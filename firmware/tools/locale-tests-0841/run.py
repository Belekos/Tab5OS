#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,tempfile
here=Path(__file__).resolve().parent;root=here.parent.parent;sketch=root/'Tab5OS_Arduino_v0.8.41_Ecriture'
p=argparse.ArgumentParser();p.add_argument('--arduinojson',type=Path,default=Path('/workspace/scratch/644cc83c43c8/build_0841/sketchbook/libraries/ArduinoJson/src'));p.add_argument('--reader-header',type=Path);a=p.parse_args()
names=['Tab5Localization.cpp','Tab5Localization.h','Tab5LanguageReader.h','Tab5LanguageMetadata.h','Tab5TranslationFormat.h','Tab5EnglishFallback.h','Tab5EnglishBitmap.h','Tab5Json.h','Tab5JsonFile.h']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
hashes={str((sketch/n).relative_to(root)):sha(sketch/n) for n in names}
hashes.update({str(p.relative_to(root)):sha(p) for p in (sketch/'sdcard/System/languages').glob('*.json')})
proof=root/('tools/verification/locale-candidate-0841' if a.reader_header else 'tools/verification/locale-0841');proof.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='tab5-locale-0841-') as temp:
 w=Path(temp)
 for n in names:shutil.copyfile(a.reader_header if a.reader_header and n=='Tab5LanguageReader.h' else sketch/n,w/n)
 for n in ['Platform.h','locale_test.cpp']:shutil.copyfile(here/n,w/n)
 for n in ['Arduino.h','FS.h','Preferences.h','SD.h','Tab5VolumeFS.h','esp_heap_caps.h','freertos/FreeRTOS.h','freertos/semphr.h']:
  dest=w/n;dest.parent.mkdir(exist_ok=True,parents=True);dest.write_text('#pragma once\n#include "Platform.h"\n')
 cmd=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Wno-misleading-indentation','-I'+str(w),'-I'+str(a.arduinojson),str(w/'Tab5Localization.cpp'),str(w/'locale_test.cpp'),'-o',str(w/'test')]
 r=subprocess.run(cmd,text=True,capture_output=True);(proof/'build.log').write_text(r.stdout+r.stderr)
 if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
 r=subprocess.run([str(w/'test'),str(sketch/'sdcard/System/languages')],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
 (proof/'run.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr)
 if r.returncode:raise SystemExit(r.returncode)
 result=json.loads(r.stdout.strip().splitlines()[-1])
for name,h in hashes.items():assert sha(root/name)==h,('Source changed during tests',name)
report={'version':'0.8.41','passed':True,'physicalDeviceTested':False,'sourceStableDuringTests':True,'checks':result['checks'],'result':result,'sourceHashes':hashes,'evidenceHashes':{str(p.relative_to(root)):sha(p) for p in [here/'run.py',here/'locale_test.cpp',here/'Platform.h',*proof.iterdir()] if p.is_file()},'method':'Full production Tab5Localization.cpp and dependencies on host; all 14 current compact JSON packs selected and every translation checked against ArduinoJson. Filesystem/Preferences/semaphores doubles, tracked PSRAM allocation refusals and I/O failures.','sanitizers':['address','undefined'],'limitations':['Host read and allocation call counts, not timings on ESP32-P4.','File I/O and mutexes mocked; physical SD and UI lifetime not measured.','Custom JSON must be valid UTF-8 and have no duplicate keys as documented.']}
if a.reader_header:
 report['candidateOnly']=True;report['candidateSourceSha256']=sha(a.reader_header);report['candidateSourcePath']=str(a.reader_header);report['sourceHashes'].pop(str((sketch/'Tab5LanguageReader.h').relative_to(root)))
(root/('tools/verification/locale-candidate-0841.json' if a.reader_header else 'tools/verification/locale-0841.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
