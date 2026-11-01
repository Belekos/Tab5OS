#!/usr/bin/env python3
"""Revalidate unchanged actual display pipeline; no physical driver execution."""
from pathlib import Path
import argparse,hashlib,json,subprocess,os,re,zipfile
r=Path(__file__).resolve().parents[2];t=Path(__file__).parent;w=r.parent.parent;s=r/'Tab5OS_Arduino_v0.8.44_Ecriture';old=r/'tools/display-tests-0843'
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,default=w/'build_0844/display-media');p.add_argument('--lvgl',type=Path,default=w/'build_0842/host-dependencies/lvgl-9.6.0');p.add_argument('--lvgl-build',type=Path,default=w/'host_lvgl_0842');p.add_argument('--m5gfx',type=Path,default=w/'build_0844/qa/firmware/dependencies/M5GFX');p.add_argument('--baseline',type=Path,default=w/'Tab5OS_0.8.43_Accueil.zip');a=p.parse_args();a.build.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
initial={str(p.relative_to(r)):sha(p) for p in sorted(s.glob('Tab5Display*'))}
assert sha(a.baseline)=='a0c58c0ba1003722e3f834eac0618dc937c5395afb7cc650dd03220730595e52'
with zipfile.ZipFile(a.baseline) as z:
 for source,digest in initial.items():
  name=source.replace('Tab5OS_Arduino_v0.8.44_Ecriture','Tab5OS_Arduino_v0.8.43_Ecriture')
  key=next(n for n in z.namelist() if n.endswith('/'+name))
  assert hashlib.sha256(z.read(key)).hexdigest()==digest,source+' changed from baseline'
files=[old/'pipeline.cpp',*[s/n for n in ['Tab5DisplayRefresh.cpp','Tab5DisplayPresent.cpp','Tab5DisplayCache.cpp','Tab5DisplayActivity.cpp']]]
flags=['-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build/'lv_conf.h')+'"','-I'+str(old/'stubs'),'-I'+str(s),'-I'+str(a.lvgl)]
cmd=['c++',*flags,*map(str,files),str(a.lvgl_build/'liblvgl.a'),'-pthread','-o',str(a.build/'pipeline')]
cp=subprocess.run(cmd,capture_output=True,text=True);(a.build/'build.log').write_text(cp.stdout+cp.stderr)
if cp.returncode:print(cp.stderr[-7000:]);raise SystemExit(cp.returncode)
cp=subprocess.run([str(a.build/'pipeline')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));(a.build/'run.log').write_text(cp.stdout+cp.stderr);print(cp.stdout+cp.stderr)
if cp.returncode:raise SystemExit(cp.returncode)
assert initial=={str(p.relative_to(r)):sha(p) for p in sorted(s.glob('Tab5Display*'))}
sdkPaths=[a.m5gfx/'src/lgfx/v1'/n for n in ['Panel.hpp','LGFXBase.hpp','LGFXBase.inl','panel/Panel_FrameBufferBase.hpp','panel/Panel_FrameBufferBase.inl','platforms/esp32p4/Panel_DSI.hpp','platforms/esp32p4/Panel_DSI.inl']]
report={'version':'0.8.44','passed':True,'physicalDeviceTested':False,'displayProductionByteIdenticalTo0843':True,'counts':{k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',cp.stdout)},'sourceHashes':initial,'testSourceHashes':{str(p.relative_to(r)):sha(p) for p in [*old.rglob('*'),*t.rglob('*')] if p.is_file() and '__pycache__' not in str(p)},'reviewedM5gfxHashes':{str(p.relative_to(a.m5gfx)):sha(p) for p in sdkPaths},'lvglArchiveSha256':sha(a.lvgl_build/'liblvgl.a'),'method':'Actual .44 display modules linked to real LVGL9.6 and existing explicit CPU-cache/external-RAM/DMA/IRQ/task doubles. Read-only review of exact pinned M5GFX0.2.30 sources; no M5GFX hardware driver executed.','limitations':['No physical Tab5, hardware MIPI/DSI or actual ESP-IDF driver exercised.','Model does not establish real IRQ order, bus bandwidth, underrun behavior, panel timing or flash absence.','No new display production patch; SDK files are source-review evidence, not runtime test.','LSan disabled; ASan/UBSan active.']}
o=r/'tools/verification';o.mkdir(exist_ok=True);(o/'display-media-0844.json').write_text(json.dumps(report,indent=2)+'\n')
