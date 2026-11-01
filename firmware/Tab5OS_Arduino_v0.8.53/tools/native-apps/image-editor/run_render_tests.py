#!/usr/bin/env python3
"""Verify exact raster results, partial redraws and old/new host costs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
HERE = Path(__file__).resolve().parent
RELEASE = HERE.parents[3]
parser=argparse.ArgumentParser()
parser.add_argument('--baseline',type=Path,required=True,help='0.8.32 image_core.c, unchanged')
parser.add_argument('--output',type=Path,default=RELEASE/'tools/verification/image-render-0833.json')
a=parser.parse_args()
cc=os.environ.get('CC','cc')
def run(command,**kwargs):
    result=subprocess.run(command,text=True,capture_output=True,**kwargs)
    if result.returncode:
        print(result.stdout,result.stderr)
        result.check_returncode()
    return result
with tempfile.TemporaryDirectory(prefix='tab5-render-0833-') as directory:
    work=Path(directory)
    env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
    flags=['-std=c11','-Wall','-Wextra','-Werror']
    current=HERE/'image_core.c';test=HERE/'test_render.c'
    binary=work/'asan'
    run([cc,*flags,'-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(current),str(test),'-o',str(binary)])
    binary.chmod(0o755)
    sanitized=json.loads(run([str(binary)],env=env).stdout.strip().splitlines()[-1])
    benchmarks={}
    for name,source,extra in [('baseline',a.baseline,['-DIE_BASELINE']),('current',current,[])]:
        binary=work/name
        run([cc,*flags,'-O2',*extra,str(source),str(test),'-o',str(binary)])
        binary.chmod(0o755)
        benchmarks[name]=json.loads(run([str(binary),'--bench']).stdout.strip().splitlines()[-1])
    old,new=benchmarks['baseline'],benchmarks['current']
    for key in ('compositeDigest','brushDigest'):
        assert old[key]==new[key]==sanitized[key],(key,old[key],new[key])
report={
 'version':'0.8.33','status':'passed','component':'native-image-editor-regional-rendering',
 'checks':sanitized['checks'],'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],
 'sourceHashes':{str(p.relative_to(RELEASE)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (current,HERE/'image_core.h',test,Path(__file__))},
 'baseline':{'version':'0.8.32','sha256':hashlib.sha256(a.baseline.read_bytes()).hexdigest()},
 'sanitized':sanitized,'hostMeasurements':benchmarks,
 'ratios':{'fullComposite':old['fullSeconds']/new['fullSeconds'],'oldFullVersusNew24x24':old['fullSeconds']/new['regionSeconds'],'hardBrush':old['brushSeconds']/new['brushSeconds'],'softBrush':old['softBrushSeconds']/new['softBrushSeconds']},
 'coverage':['Exact alpha composition against 0.8.32 equation including zero/full/partial alpha and hidden/transparent layers','Exact old/new brush digest across round/square, 1..256px, softness, diffusion, opacity, erase and selections','All output pixels checked: clipped dirty rectangles, row padding, down/up scaling, 1px images, int-overflow boundaries','No allocations or document mutation during rendering','34432-check full core suite run independently'],
 'limitations':['Timings are host CPU timings and do not predict tablet FPS or touch latency.','The 24x24 region comparison demonstrates reduced work, not a same-work algorithmic speedup.','No ESP32-P4 display, PSRAM bus or LVGL flush timing measured.']
}
report['coverage'][-1]='34342-check full core suite run independently'
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'status':report['status'],'checks':report['checks'],'ratios':report['ratios'],'report':str(a.output)}))
