#!/usr/bin/env python3
"""Production mouse + LVGL; USB and editor boundary mocked; exact main route executed."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess
p=argparse.ArgumentParser();p.add_argument('--lvgl',required=True,type=Path);p.add_argument('--lvgl-build',required=True,type=Path);p.add_argument('--build',required=True,type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parent.parent;s=r/'Tab5OS_Arduino_v0.8.35_Ecriture';w=a.build.resolve();w.mkdir(parents=True,exist_ok=True)
paths=[s/n for n in ['Tab5OS_Arduino_v0.8.35_Ecriture.ino','Tab5Mouse.cpp','Tab5Mouse.h','WriterUsb.h','WriterKeys.h','WriterMouse.h','WriterGamepad.h']]+[here/'run.py',here/'input.cpp']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
original={str(p.relative_to(r)):sha(p) for p in paths}
text=(s/'Tab5OS_Arduino_v0.8.35_Ecriture.ino').read_text();start=text.index('static bool imageMouseEvent(int x,int y,uint8_t buttons,int wheel) {');end=text.index('\n}\n',start)+3;route=text[start:end];(w/'image-route.inc').write_text(route)
flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DLV_CONF_PATH="'+str(a.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(s),'-I'+str(w),'-I'+str(a.lvgl.resolve()),'-I'+str(a.lvgl.resolve()/'include')]
cmd=['g++',*flags,str(here/'input.cpp'),str(a.lvgl_build.resolve()/'liblvgl.a'),'-pthread','-o',str(w/'input')]
b=subprocess.run(cmd,capture_output=True,text=True);(w/'build.log').write_text(b.stdout+b.stderr)
if b.returncode:print(b.stdout+b.stderr);raise SystemExit(b.returncode)
t=subprocess.run([str(w/'input')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));print(t.stdout+t.stderr);(w/'run.log').write_text(t.stdout+t.stderr)
assert t.returncode==0
assert original=={str(p.relative_to(r)):sha(p) for p in paths}
m=re.search(r'PASS: (\d+) actual LVGL mouse assertions; (\d+) failures',t.stdout);assert m
report=dict(version='0.8.35',passed=True,physicalDeviceTested=False,method=__doc__,sourceHashes=original,sanitizers=['address','undefined'],result=dict(checks=int(m[1]),failures=int(m[2]),contextCycles=1000),extractedRouteSha256=hashlib.sha256(route.encode()).hexdigest(),lvglArchiveSha256=sha(a.lvgl_build/'liblvgl.a'),limitations=['USB queue and editor mouse boundary mocked. Native editor actions tested separately. No physical USB/display tested.'])
(r/'tools/verification/drawing-input-0835.json').write_text(json.dumps(report,indent=2)+'\n')
