#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,subprocess,os,shutil
r=Path(__file__).resolve().parents[2];t=Path(__file__).parent;s=r/'Tab5OS_Arduino_v0.8.49_Ecriture';workspace=r.parent.parent
b=workspace/'build_0849/media-hide-native';b.mkdir(parents=True,exist_ok=True)
lvgl=workspace/'lvgl_allocator_prototype_0849/upstream/lvgl-9.6.0';lib=workspace/'build_drawing_0848/lvgl'
names=['Tab5MediaList.cpp','Tab5MediaList.h','Tab5UiStyle.cpp','Tab5UiStyle.h','Tab5TextInput.cpp','Tab5TextInput.h','Tab5Focus.cpp','Tab5Focus.h','WriterDocument.cpp','WriterDocument.h','WriterKeys.h','WriterEncoding.h','Tab5FileSort.h']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
before={n:sha(s/n) for n in names}
for n in names:shutil.copyfile(s/n,b/n)
shutil.copyfile(r/'tools/media-list-tests-0846/esp_heap_caps.h',b/'esp_heap_caps.h')
for name in ['Arduino.h','Tab5Localization.h']:(b/name).write_text('#include "Platform.h"\n')
flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-DLV_CONF_PATH="'+str(lib/'lv_conf.h')+'"','-I'+str(b),'-I'+str(t),'-I'+str(r/'tools/media-list-tests-0846'),'-I'+str(lvgl),'-Wno-cpp','-Wno-deprecated-declarations']
cmd=['g++',*flags,str(t/'native.cpp'),str(b/'WriterDocument.cpp'),str(b/'Tab5Focus.cpp'),str(lib/'liblvgl.a'),'-pthread','-o',str(b/'test')]
cp=subprocess.run(cmd,capture_output=True,text=True);(b/'build.log').write_text(cp.stdout+cp.stderr)
if cp.returncode:print(cp.stderr[-7000:]);raise SystemExit(cp.returncode)
cp=subprocess.run([str(b/'test')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));(b/'run.log').write_text(cp.stdout+cp.stderr);print(cp.stdout+cp.stderr)
if cp.returncode:raise SystemExit(cp.returncode)
assert before=={n:sha(s/n) for n in names},'Source changed during tests'
report={'version':'0.8.49','passed':True,'physicalDeviceTested':False,'method':'Entire actual MediaList, Focus, TextInput, UiStyle and WriterDocument linked to real LVGL9.6, 800-reference integration plus 100 hide/destruct/recreate cycles with remembered focus and scroll/size event dispatch after destruction. ASan/UBSan.','output':cp.stdout,'sourceHashes':{str((s/n).relative_to(r)):digest for n,digest in before.items()},'testSourceHashes':{str(p.relative_to(r)):sha(p) for p in t.rglob('*') if p.is_file() and '__pycache__' not in str(p)},'lvglArchiveSha256':sha(lib/'liblvgl.a'),'limitations':['No physical Tab5, audio decoder or file driver.','LSan disabled; ASan/UBSan active.']}
(r/'tools/verification/media-hide-native-0849.json').write_text(json.dumps(report,indent=2)+'\n')
