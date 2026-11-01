#!/usr/bin/env python3
"""Real USB-report service and context menu on LVGL, compared with delivered .52."""
import argparse, hashlib, json, os, re, subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
for key in ('baseline','lvgl','lvgl-build','build'):
    parser.add_argument('--'+key,type=Path,required=True)
args=parser.parse_args()
sketch=ROOT/'Tab5OS_Arduino_v0.8.53_Ecriture'
sources=['Tab5Mouse.cpp','Tab5ContextMenu.cpp','Tab5Focus.cpp','Tab5TextInput.cpp','Tab5UiStyle.cpp','WriterDocument.cpp']
headers=['Tab5Mouse.h','Tab5ContextMenu.h','Tab5Focus.h','Tab5TextInput.h','Tab5UiStyle.h','WriterDocument.h','WriterKeys.h','WriterUsb.h','WriterMouse.h']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
evidence=ROOT/'tools/verification/context-hover-0853'
evidence.mkdir(exist_ok=True)
source_hashes={str((sketch/n).relative_to(ROOT)):sha(sketch/n) for n in sources+headers}
test_hashes={str(p.relative_to(ROOT)):sha(p) for p in [HERE/'main.cpp',HERE/'run.py',ROOT/'tools/explorer-tests-0837/context_menu.cpp']}
environment=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
results={}
for mode,source in [('baseline',args.baseline),('current',sketch)]:
    build=args.build/mode;build.mkdir(parents=True,exist_ok=True)
    flags=['-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-Wno-deprecated-declarations','-Wno-cpp',
           '-DLV_CONF_PATH="'+str(args.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(source.resolve()),'-I'+str(args.lvgl.resolve())]
    cmd=['g++',*flags,str(HERE/'main.cpp'),*[str(source/n) for n in sources if n!='Tab5Mouse.cpp'],str(args.lvgl_build/'liblvgl.a'),'-o',str(build/'test')]
    compiled=subprocess.run(cmd,capture_output=True,text=True,timeout=180)
    (evidence/(mode+'-compile.log')).write_text(compiled.stdout+compiled.stderr)
    if compiled.returncode:print(compiled.stdout+compiled.stderr);compiled.check_returncode()
    ran=subprocess.run([str(build/'test'),mode,str(evidence)],capture_output=True,text=True,env=environment,timeout=60)
    (evidence/(mode+'-run.log')).write_text(ran.stdout+ran.stderr)
    if ran.returncode:print(ran.stdout+ran.stderr);ran.check_returncode()
    results[mode]={'result':json.loads(ran.stdout.strip().splitlines()[-1]),'compileExitCode':compiled.returncode,'runExitCode':ran.returncode,'command':cmd,
                   'sourceHashes':{n:sha(source/n) for n in sources+headers}}
    print(mode,results[mode]['result'])

# Re-execute established menu lifetime/editing cases on the current source.
build=args.build/'lifecycle';build.mkdir(parents=True,exist_ok=True)
cmd=['g++',*flags,str(ROOT/'tools/explorer-tests-0837/context_menu.cpp'),*[str(sketch/n) for n in ['Tab5Focus.cpp','WriterDocument.cpp']],str(args.lvgl_build/'liblvgl.a'),'-o',str(build/'test')]
compiled=subprocess.run(cmd,capture_output=True,text=True,timeout=180)
(evidence/'lifecycle-compile.log').write_text(compiled.stdout+compiled.stderr)
if compiled.returncode:print(compiled.stdout+compiled.stderr);compiled.check_returncode()
ran=subprocess.run([str(build/'test')],capture_output=True,text=True,env=environment,timeout=60)
(evidence/'lifecycle-run.log').write_text(ran.stdout+ran.stderr)
if ran.returncode:print(ran.stdout+ran.stderr);ran.check_returncode()
summary=ran.stdout.strip().splitlines()[-1]
assert re.match(r'PASS \d+ context-menu checks;',summary),summary
results['lifecycle']={'summary':summary,'compileExitCode':0,'runExitCode':0,'command':cmd}
assert source_hashes=={str((sketch/n).relative_to(ROOT)):sha(sketch/n) for n in sources+headers},'production changed during tests'
assert test_hashes=={str(p.relative_to(ROOT)):sha(p) for p in [HERE/'main.cpp',HERE/'run.py',ROOT/'tools/explorer-tests-0837/context_menu.cpp']},'tests changed during run'
from PIL import Image
for p in evidence.glob('*.ppm'):Image.open(p).save(p.with_suffix('.png'));p.unlink()
report={'version':'0.8.53','passed':True,'physicalDeviceTested':False,'scope':__doc__,'sourceStableDuringTests':True,
        'sourceHashes':source_hashes,'testSourceHashes':test_hashes,'results':results,
        'evidenceHashes':{str(p.relative_to(ROOT)):sha(p) for p in sorted(evidence.iterdir())},
        'lvglArchiveSha256':sha(args.lvgl_build/'liblvgl.a'),'lvglConfigSha256':sha(args.lvgl_build/'lv_conf.h'),
        'sanitizers':{'address':True,'undefined':True,'leakDetection':False},
        'limitations':['USB transport is simulated; real Mouse::service, ContextMenu, Focus, UiStyle and LVGL are executed.',
                      'Menu commands count callbacks; file IO and full Explorer page are outside this harness.',
                      'Hardware timing, MIPI display and physical USB/touch are not measured.',
                      'Lifecycle regression sends logical LVGL actions directly; hover test uses actual relative USB report dispatch.']}
(ROOT/'tools/verification/context-hover-0853.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(summary)
