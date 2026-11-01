#!/usr/bin/env python3
from pathlib import Path
import argparse,subprocess,json,hashlib,os
p=argparse.ArgumentParser();p.add_argument('--build',required=True,type=Path);a=p.parse_args();here=Path(__file__).resolve().parent;release=here.parents[1];sketch=release/'Tab5OS_Arduino_v0.8.48_Ecriture';a.build.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();binary=a.build.resolve()/'review-pending';flags=['-std=c++17','-Wall','-Wextra','-Werror','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie'];inputs=[sketch/'Tab5DisplayPendingMirror.h',sketch/'Tab5DisplayCacheRanges.h'];tests=[here/'review_pending_mirror.cpp',Path(__file__).resolve()];before={p:sha(p) for p in inputs+tests}
subprocess.run(['c++',*flags,'-I',str(sketch),str(tests[0]),'-o',str(binary)],check=True)
r=subprocess.run([str(binary)],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),check=True)
assert before=={p:sha(p) for p in inputs+tests}
out={'version':'0.8.48',**json.loads(r.stdout),'physicalDeviceTested':False,'sourceHashes':{str(p.relative_to(release)):before[p] for p in inputs},'testSourceHashes':{str(p.relative_to(release)):before[p] for p in tests},'flags':flags,'method':'Independent exhaustive pixel oracle: 474552 combinations, no copy may overwrite a previously written pixel. Only pure pending-span arithmetic executed.'}
(release/'tools/verification/display-pending-independent-0848.json').write_text(json.dumps(out,indent=2)+'\n');print(r.stdout)
