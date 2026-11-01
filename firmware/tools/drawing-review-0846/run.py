#!/usr/bin/env python3
from pathlib import Path
import subprocess,zipfile,hashlib,json,os
h=Path(__file__).resolve().parent;r=h.parents[1];s=r/'Tab5OS_Arduino_v0.8.46_Ecriture';w=r.parent.parent/'build_0846/drawing-review';w.mkdir(parents=True,exist_ok=True);o=r/'tools/verification/drawing-review-0846';o.mkdir(parents=True,exist_ok=True)
z=r.parent.parent/'Tab5OS_0.8.45_SD_Radio.zip';assert hashlib.sha256(z.read_bytes()).hexdigest()=='46fd5da89ebb693a61d3ab34e2d3f3480a714aff76c095ce339f290ee8641573'
b=w/'baseline';b.mkdir(exist_ok=True)
with zipfile.ZipFile(z) as f:
 for n in f.namelist():
  if '/tools/native-apps/image-editor/' in n and Path(n).name in ['image_core.c','image_core.h','image_selection.h']:(b/Path(n).name).write_bytes(f.read(n))
current=s/'tools/native-apps/image-editor';records={}
production=[current/'image_core.c',current/'image_core.h',current/'image_selection.h']
initial={str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in production}
for name,source in [('baseline',b),('current',current)]:
 cmd=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-I'+str(source),str(h/'review.c'),str(source/'image_core.c'),'-o',str(w/name)]
 # Do not overwrite baseline source directory with executable.
 cmd[-1]=str(w/(name+'-test'))
 p=subprocess.run(cmd,capture_output=True,text=True);(o/(name+'-build.log')).write_text(p.stdout+p.stderr);p.check_returncode()
 p=subprocess.run([cmd[-1]],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));(o/(name+'-run.log')).write_text(p.stdout+p.stderr);p.check_returncode();records[name]={'stdout':p.stdout,'stderr':p.stderr,'sha256':hashlib.sha256(p.stdout.encode()).hexdigest()};print(name,p.stderr)
assert records['baseline']['stdout']==records['current']['stdout'],'Different document, rendering, undo or redo result'
assert initial=={str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in production},'Source changed during differential review'
report={'sourceStableDuringTest':True,'version':'0.8.46','passed':True,'physicalDeviceTested':False,'operations':5000,'knownRGB565PrimaryPixelsChecked':4608,'sameDocumentMetadataPixelsRenderingUndoRedoAfterEveryOperation':True,'baselineZipSha256':hashlib.sha256(z.read_bytes()).hexdigest(),'sequenceSha256':records['current']['sha256'],'sourceHashes':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [current/'image_core.c',current/'image_core.h',current/'image_selection.h']},'testSourceHashes':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in h.glob('*') if p.is_file()},'evidenceHashes':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in o.glob('*') if p.is_file()},'scope':'Independent differential review: 5000 deterministic mixed public operations, layers/order/style/names/geometry/selection-mask, brushes+eraser+warp+text+cancel+undo+redo; document pixels, metadata and RGB565 viewport bytes compared with authenticated0.8.45 after each step. Host allocator poisons new bytes0xa5; ASan/UBSan; zero tracked live bytes afterdestroy. No memory-fault injection in this differential test.'}
(r/'tools/verification/drawing-review-0846.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'passed':True,'sequenceSha256':report['sequenceSha256']}))
