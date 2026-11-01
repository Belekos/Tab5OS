#!/usr/bin/env python3
"""Compile the actual Media list against pinned LVGL, then compare delivered .46."""
from pathlib import Path
import hashlib,json,subprocess,os,shutil,zipfile
r=Path(__file__).resolve().parents[2];t=Path(__file__).parent
s=r/'Tab5OS_Arduino_v0.8.47_Ecriture';workspace=r.parent.parent
b=workspace/'build_0847/media-ui';b.mkdir(parents=True,exist_ok=True)
lvgl=workspace/'build_0847/sketchbook/libraries/lvgl';lib=workspace/'build_0847/host-lvgl'
names=['Tab5MediaList.cpp','Tab5MediaList.h','Tab5UiStyle.cpp','Tab5UiStyle.h','Tab5TextInput.cpp','Tab5TextInput.h','Tab5Focus.cpp','Tab5Focus.h','WriterDocument.cpp','WriterDocument.h','WriterKeys.h','WriterEncoding.h','Tab5FileSort.h','Tab5Search.h']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
before={n:sha(s/n) for n in names};results={};integration_results={}
main=s/'Tab5OS_Arduino_v0.8.47_Ecriture.ino'
main_sha=sha(main)
archive=workspace/'baseline_0846/Tab5OS_0.8.46_Reseau_Memoire.zip'
assert sha(archive)=='22d99628cc3ab6a9673be1886755ec9a3092aaede3f017a7e0e20443cc0816a6'
def function(source,name):
 import re
 match=re.search(r'static [^\n;{}]+\b'+name+r'\([^;]*?\)\s*\{',source)
 if not match: raise ValueError(name)
 start=match.start();pos=source.index('{',match.start());depth=0;quote=None;escape=False
 for i in range(pos,len(source)):
  ch=source[i]
  if quote:
   if escape:escape=False
   elif ch=='\\':escape=True
   elif ch==quote:quote=None
   continue
  if ch in ['"',"'"]:quote=ch;continue
  if ch=='{':depth+=1
  elif ch=='}':
   depth-=1
   if not depth:return source[start:i+1]
 raise ValueError(name+' unbalanced')
for version in ['baseline','current']:
 out=b/version;out.mkdir(parents=True,exist_ok=True)
 if version=='baseline':
  with zipfile.ZipFile(archive) as z:
   for n in names:(out/n).write_bytes(z.read('Tab5OS_Ecriture_v0.8.46/Tab5OS_Arduino_v0.8.46_Ecriture/'+n))
   main_text=z.read('Tab5OS_Ecriture_v0.8.46/Tab5OS_Arduino_v0.8.46_Ecriture/Tab5OS_Arduino_v0.8.46_Ecriture.ino').decode()
 else:
  for n in names:shutil.copyfile(s/n,out/n)
  main_text=main.read_text();main_sha=sha(main)
 shutil.copyfile(t/'esp_heap_caps.h',out/'esp_heap_caps.h')
 for name in ['Arduino.h','Tab5Localization.h']:(out/name).write_text('#include "Platform.h"\n')
 flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-DCURRENT='+str(int(version=='current')),'-DLV_CONF_PATH="'+str(lib/'lv_conf.h')+'"','-I'+str(out),'-I'+str(t),'-I'+str(lvgl),'-Wno-cpp','-Wno-deprecated-declarations']
 cmd=['g++',*flags,str(t/'list.cpp'),str(out/'WriterDocument.cpp'),str(out/'Tab5Focus.cpp'),str(lib/'liblvgl.a'),'-pthread','-o',str(out/'test')]
 cp=subprocess.run(cmd,capture_output=True,text=True);(out/'build.log').write_text(cp.stdout+cp.stderr)
 if cp.returncode:print(cp.stderr[-7000:]);raise SystemExit(cp.returncode)
 cp=subprocess.run([str(out/'test')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));(out/'run.log').write_text(cp.stdout+cp.stderr)
 print(version,cp.stdout+cp.stderr)
 if cp.returncode:raise SystemExit(cp.returncode)
 results[version]=json.loads(cp.stdout)
 functions=['startVideoViewer','audioPathIsRecording','audioStationCategory','addUniqueAudioGroup','sortAudioGroups','setAudioTabState','audioRestoreBrowser','audioRememberBrowser','audioRememberScroll','audioRememberItem','audioGroupBackEvent','audioFavoriteId','audioChildGroup','audioMediaMetadata','audioMediaFormat','audioMediaAction','refreshAudioList','audioLocalTabEvent','audioRadioTabEvent','audioRecordTabEvent','audioVideoTabEvent']
 if version=='current':functions[functions.index('audioRememberItem'):functions.index('audioRememberItem')]=['audioCatalogueRevision','audioModelCurrent']
 code='\n\n'.join(function(main_text,name) for name in functions)
 ui_prefix=function(main_text,'updateAudioUi').split('  static uint32_t seen = 0;')[0]+'\n}\n'
 code+='\n'+ui_prefix
 create=function(main_text,'createAudioPage');blocks=[]
 for name in ['audioLocalTab','audioRadioTab','audioRecordTab','audioVideoTab']:
  start=create.index('  '+name+' =');end=create.index('\n',create.index('lv_obj_add_event_cb('+name,start))
  blocks.append(create[start:end])
 code+='\nstatic void createExtractedTabs(lv_obj_t* sourcePanel){\n'+'\n'.join(blocks)+'\n}\n'
 (out/'integration-extracted.inc').write_text(code)
 cmd=['g++',*flags,str(t/'integration.cpp'),str(out/'WriterDocument.cpp'),str(out/'Tab5Focus.cpp'),str(lib/'liblvgl.a'),'-pthread','-o',str(out/'integration')]
 cp=subprocess.run(cmd,capture_output=True,text=True);(out/'integration-build.log').write_text(cp.stdout+cp.stderr)
 if cp.returncode:print(cp.stderr[-7000:]);raise SystemExit(cp.returncode)
 cp=subprocess.run([str(out/'integration')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));(out/'integration-run.log').write_text(cp.stdout+cp.stderr);print(version,'integration',cp.stdout+cp.stderr)
 if cp.returncode:raise SystemExit(cp.returncode)
 integration_results[version]=json.loads(cp.stdout)
revision_header_sha=sha(s/'Tab5CatalogueRevision.h')
revision_binary=b/'revision-test'
cp=subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-pthread','-I'+str(s),str(t/'catalogue_revision.cpp'),'-o',str(revision_binary)],capture_output=True,text=True)
(b/'revision-build.log').write_text(cp.stdout+cp.stderr)
if cp.returncode:print(cp.stderr);raise SystemExit(cp.returncode)
cp=subprocess.run([str(revision_binary)],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
(b/'revision-run.log').write_text(cp.stdout+cp.stderr);print(cp.stdout+cp.stderr)
if cp.returncode:raise SystemExit(cp.returncode)
assert revision_header_sha==sha(s/'Tab5CatalogueRevision.h'),'Revision header changed during tests'
assert before=={n:sha(s/n) for n in names},'Source changed during tests'
final_main=main.read_text()
for name in functions:
 assert function(main_text,name)==function(final_main,name),'Tested main function changed: '+name
final_create=function(final_main,'createAudioPage')
for name,block in zip(['audioLocalTab','audioRadioTab','audioRecordTab','audioVideoTab'],blocks):
 start=final_create.index('  '+name+' =');end=final_create.index('\n',final_create.index('lv_obj_add_event_cb('+name,start))
 assert block==final_create[start:end],'Tested tab construction changed: '+name
fragment_hashes={name:hashlib.sha256(function(final_main,name).encode()).hexdigest() for name in functions}
assert function(final_main,'updateAudioUi').split('  static uint32_t seen = 0;')[0]+'\n}\n'==ui_prefix,'Catalogue service prefix changed'
fragment_hashes['updateAudioUi-catalogue-prefix']=hashlib.sha256(ui_prefix.encode()).hexdigest()
fragment_hashes['tab-construction']=hashlib.sha256('\n'.join(blocks).encode()).hexdigest()
report={'version':'0.8.47','passed':True,'physicalDeviceTested':False,'method':'Actual production Media list, actual pinned LVGL 9.6 software rendering, controlled heap and catalogues; rendered pixels and committed references, not source-text checks.','boundary':'No physical DSI, I2S or SD. Explicit PSRAM allocator tracking excludes ordinary LVGL/libc allocations. LSan disabled; ASan and UBSan enabled.','sourceSha256':dict(before,**{'Tab5CatalogueRevision.h':revision_header_sha}),'baselineZipSha256':sha(archive),'results':results,'mainIntegrationResults':integration_results,'mainSourceSha256AtExtraction':main_sha,'verifiedMainFragmentSha256':fragment_hashes,'hostLvglSha256':sha(lib/'liblvgl.a')}
v=r/'tools/verification';v.mkdir(exist_ok=True);(v/'media-ui-0847.json').write_text(json.dumps(report,indent=2)+'\n')
for version in results:
 out=v/'media-ui-0847'/version;out.mkdir(parents=True,exist_ok=True)
 for name in ['build.log','run.log','integration-build.log','integration-run.log','integration-extracted.inc']:shutil.copyfile(b/version/name,out/name)
for name in ['revision-build.log','revision-run.log']:shutil.copyfile(b/name,v/'media-ui-0847'/name)
print('PASS exact production sources verified; report:',v/'media-ui-0847.json')
