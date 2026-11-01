from pathlib import Path
import json,hashlib,subprocess,os,sys,shutil
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.25_Ecriture'
build=Path('C:/workspace/tab5-directory-0825');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
 cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-ui-0819-host/lvgl.lib')
files=['Tab5DirectoryView.cpp','Tab5DirectoryView.h','Tab5DirectoryModel.h','Tab5FilePicker.cpp','Tab5FilePicker.h','Tab5FileSort.h']
for name in files:shutil.copy2(sketch/name,build/name)
for name in ['Platform.h','directory.cpp']:shutil.copy2(here/name,build/name)
for name in ['FS.h','SD.h','Tab5VolumeFS.h','Tab5HiddenFiles.h','Tab5Localization.h','Tab5Storage.h','Tab5UsbStorage.h','esp_heap_caps.h']:(build/name).write_text('#include "Platform.h"\n',encoding='utf-8')
flags=['/nologo','/utf-8','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_LABEL_TEXT_SELECTION=1','/DLV_USE_OS=0','/D_CRT_SECURE_NO_WARNINGS','/I'+str(build),'/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch)]
command=['cl',*flags,'/EHsc','/std:c++17',str(build/'directory.cpp'),str(sketch/'WriterDocument.cpp'),str(sketch/'Tab5Focus.cpp'),str(library),'/Fe:'+str(build/'directory.exe')]
c=subprocess.run(command,cwd=build,capture_output=True);log=c.stdout.decode('utf-8','replace')+c.stderr.decode('utf-8','replace');print(log)
if c.returncode:raise SystemExit(c.returncode)
r=subprocess.run([str(build/'directory.exe')],cwd=build,capture_output=True);text=r.stdout.decode('utf-8','replace')+r.stderr.decode('utf-8','replace');print(text)
names=files+['Tab5Writing.cpp','Tab5OS_Arduino_v0.8.25_Ecriture.ino','Tab5UiStyle.cpp','Tab5Focus.cpp']
report={'version':'0.8.25','passed':r.returncode==0,'exitCode':r.returncode,'output':text,'method':'Actual LVGL9.6 + production DirectoryView, DirectoryModel, FilePicker and Focus; simulated FAT/USB media, six sort orders, selection after sorting; no device access. Editor/Settings integration checked by target compilation.','sources':{name:hashlib.sha256((sketch/name).read_bytes()).hexdigest() for name in names}}
(release/'tools/verification/directory-picker-0825.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(r.returncode)
