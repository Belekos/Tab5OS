#!/usr/bin/env python3
"""Production marker overlay and mesh badge rendered with LVGL9.6; deterministic Map/PSRAM providers."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
p=argparse.ArgumentParser();p.add_argument('--lvgl',type=Path,default=Path('/workspace/scratch/644cc83c43c8/build_0840/host-dependencies/lvgl-9.6.0'));p.add_argument('--lvgl-build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/host_lvgl_0840'));p.add_argument('--build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/build_0840/map-overlay-tests'));p.add_argument('--draft',action='store_true');a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parent.parent;s=r/'Tab5OS_Arduino_v0.8.40_Ecriture';w=a.build.resolve();w.mkdir(parents=True,exist_ok=True);shots=r/'tools/verification/map-overlay-0840';shots.mkdir(parents=True,exist_ok=True)
sourceNames=['Tab5MapOverlay.h','Tab5MapOverlay.cpp','Tab5MapMarkerProjection.h','Tab5MeshBadge.h','Tab5MeshBadge.cpp','Tab5UiStyle.h','Tab5UiStyle.cpp','Tab5Focus.h','Tab5Focus.cpp','Tab5TextInput.h','Tab5TextInput.cpp','WriterDocument.h','WriterDocument.cpp','WriterKeys.h','WriterEncoding.h','lv_conf.h']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
paths=[s/n for n in sourceNames];hashes={str(p.relative_to(r)):sha(p) for p in paths};tests={str(p.relative_to(r)):sha(p) for p in here.rglob('*') if p.is_file()}
for n in sourceNames:shutil.copy2(s/n,w/n)
for pth in (here/'stubs').iterdir():shutil.copy2(pth,w/pth.name)
flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build/'lv_conf.h')+'"','-I'+str(w),'-I'+str(a.lvgl),'-I'+str(a.lvgl/'include')]
cmd=['g++',*flags,str(here/'ui.cpp'),*[str(w/n) for n in sourceNames if n.endswith('.cpp')],str(a.lvgl_build/'liblvgl.a'),'-pthread','-o',str(w/'ui')]
b=subprocess.run(cmd,text=True,capture_output=True);(w/'build.log').write_text(b.stdout+b.stderr)
if b.returncode:print(b.stdout+b.stderr);raise SystemExit(b.returncode)
t=subprocess.run([str(w/'ui'),str(shots)],text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),timeout=180);print(t.stdout+t.stderr);(shots/'run.log').write_text(t.stdout+t.stderr)
if t.returncode:raise SystemExit(t.returncode)
from PIL import Image
for ppm in shots.glob('*.ppm'):Image.open(ppm).save(ppm.with_suffix('.png'));ppm.unlink()
result=json.loads(t.stdout.strip().splitlines()[-1]);stable=hashes=={str(p.relative_to(r)):sha(p) for p in paths}
if not a.draft:assert stable
report=dict(version='0.8.40',passed=stable and not a.draft,draft=a.draft,physicalDeviceTested=False,sourceStableDuringTests=stable,sourceHashes=hashes,testSourceHashes=tests,checks=result['checks'],result=result,lvglArchiveSha256=sha(a.lvgl_build/'liblvgl.a'),evidenceHashes={str(p.relative_to(r)):sha(p) for p in sorted(shots.rglob('*')) if p.is_file()},limitations=['Real production LVGL drawing, geometry, hit testing, badge and invalidation paths. Map model and PSRAM allocator are deterministic host mocks; synthetic map background is not a downloaded tile. No tablet, BLE radio or physical display test.','Map pins are an interactive LVGL layer; current raster export/printing continues to export map tiles without the marker layer.'])
(r/'tools/verification/map-overlay-0840.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
