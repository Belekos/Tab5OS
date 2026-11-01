#!/usr/bin/env python3
"""Actual Mesh badge, main service gate and shell title layout with real LVGL; radio counters are fixtures."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--lvgl',type=Path,default=Path('/workspace/scratch/644cc83c43c8/build_0841/host-dependencies/lvgl-9.6.0'));p.add_argument('--lvgl-build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/host_lvgl_0841'));p.add_argument('--build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/build_0841/badge-tests'));a=p.parse_args()
t=Path(__file__).resolve().parent;r=t.parent.parent;s=r/'Tab5OS_Arduino_v0.8.41_Ecriture';w=a.build;w.mkdir(parents=True,exist_ok=True);o=r/'tools/verification/badge-0841';o.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(t.parent/'settings-ui-tests-0841'));from extract import extract,closing
shell=s/'Tab5ShellUi.h';main=s/(s.name+'.ino');mesh=s/'Tab5Meshtastic.cpp'
def portions():
 text=main.read_text();start=text.index('  {const auto badge=Tab5Meshtastic::badge();');start=text.index('{',start);service=text[start:closing(text,start)]
 text=mesh.read_text();start=text.index('Badge badge(){');badge=text[start:closing(text,text.index('{',start))]
 match=re.search(r'enum class Status:[^\n]+;',text);assert match
 return {'shellAtMainMenu':extract(shell.read_text(),'shellAtMainMenu'),'shellTopLayout':extract(shell.read_text(),'shellTopLayout'),'service':service,'badge':badge,'status':match.group(0)}
parts=portions();(w/'shell-top.inc').write_text(parts['shellAtMainMenu']+'\n'+parts['shellTopLayout']);(w/'badge-status.inc').write_text(parts['status']);(w/'badge-model.inc').write_text(parts['badge']);(w/'badge-service.inc').write_text('static void serviceBadge(){'+parts['service']+'}\n')
names=['Tab5MeshBadge.h','Tab5MeshBadge.cpp','Tab5Meshtastic.h','Tab5MeshModel.h','Tab5MeshtasticProtocol.h','Tab5TopBarLayout.h','Tab5ShellLayout.h','Tab5ShellPolicy.h','Tab5UiStyle.h','Tab5UiStyle.cpp','Tab5Focus.h','Tab5Focus.cpp','Tab5TextInput.h','Tab5TextInput.cpp','WriterDocument.h','WriterDocument.cpp','WriterKeys.h','WriterEncoding.h','lv_conf.h','sdcard/System/font.ttf']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();sources={str((s/n).relative_to(r)):sha(s/n) for n in names};tests={str(p.relative_to(r)):sha(p) for p in t.rglob('*') if p.is_file() and '__pycache__' not in p.parts}
flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build/'lv_conf.h')+'"','-I'+str(w),'-I'+str(t/'mocks'),'-I'+str(s),'-I'+str(a.lvgl),'-I'+str(a.lvgl/'include')]
cmd=['g++',*flags,str(t/'ui.cpp'),*[str(s/n) for n in names if n.endswith('.cpp')],str(a.lvgl_build/'liblvgl.a'),'-pthread','-o',str(w/'ui')]
b=subprocess.run(cmd,text=True,capture_output=True);(o/'build.log').write_text(b.stdout+b.stderr)
if b.returncode:print(b.stdout+b.stderr);raise SystemExit(b.returncode)
x=subprocess.run([str(w/'ui'),str(o),str(s/'sdcard/System/font.ttf')],text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),timeout=120);print(x.stdout+x.stderr);(o/'run.log').write_text(x.stdout+x.stderr);x.check_returncode()
from PIL import Image
for ppm in o.glob('*.ppm'):Image.open(ppm).save(ppm.with_suffix('.png'));ppm.unlink()
assert parts==portions(),'tested integration functions changed'
assert sources=={str((s/n).relative_to(r)):sha(s/n) for n in names},'tested sources changed'
assert tests=={str(p.relative_to(r)):sha(p) for p in t.rglob('*') if p.is_file() and '__pycache__' not in p.parts},'tests changed'
report=dict(version='0.8.41',passed=True,physicalDeviceTested=False,method=__doc__,result=json.loads(x.stdout.strip().splitlines()[-1]),sourceHashes=sources,integrationSourceHashes={str(p.relative_to(r)):sha(p) for p in [main,shell,mesh]},integrationFunctionHashes={n:hashlib.sha256(v.encode()).hexdigest() for n,v in parts.items()},testSourceHashes=tests,evidenceHashes={str(p.relative_to(r)):sha(p) for p in o.iterdir() if p.is_file()},lvglArchiveSha256=sha(a.lvgl_build/'liblvgl.a'),limitations=['BLE states and running flag are boundary fixtures. Actual badge decision, unread ring model, display widget, service revision gate and shell title/date layout execute on host.','No complete firmware main loop, physical display or BLE radio executes. Dock positioning itself is a counted boundary stub; title reclaim uses the actual shellTopLayout function.'])
(o.parent/'badge-0841.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
