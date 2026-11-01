#!/usr/bin/env python3
"""Execute actual Meshtastic controller/UI and its OS keyboard hook on host LVGL.

ESP/NimBLE/SD are simulated boundaries. The OS hook is extracted verbatim;
the global keyboard renderer is represented by LVGL's native keyboard and a
documented visibility/target adapter, not the full running Tab5OS shell.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, subprocess

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def execute(sketch, work, evidence, lvgl, lvgl_build, fixed):
    work.mkdir(parents=True, exist_ok=True); evidence.mkdir(parents=True, exist_ok=True)
    names = ['Tab5Meshtastic.cpp','Tab5Meshtastic.h','Tab5MeshtasticProtocol.h','Tab5MeshModel.h',
             'Tab5MeshMemory.h','Tab5MeshAllocator.h','Tab5MeshStore.h','Tab5MeshCodec.h',
             'Tab5Focus.cpp','Tab5Focus.h','Tab5TextInput.cpp','Tab5TextInput.h',
             'Tab5UiStyle.cpp','Tab5UiStyle.h','WriterDocument.cpp','WriterDocument.h',
             'WriterKeys.h','sdcard/System/font.ttf']
    hashes = {name:sha(sketch/name) for name in names}
    source = (sketch/'Tab5Meshtastic.cpp').read_text()
    (work/'production-meshtastic.inc').write_text(source[source.index('namespace Tab5Meshtastic {'):])
    ino = next(sketch.glob('*.ino'))
    hook = next(line.strip() for line in ino.read_text().splitlines() if line.strip().startswith('meshHooks.keyboard='))
    (work/'production-keyboard-hook.inc').write_text(hook+'\n')
    flags = ['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',
             '-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(lvgl_build/'lv_conf.h')+'"',
             '-I'+str(sketch),'-iquote',str(work),'-I'+str(HERE),'-I'+str(lvgl)]
    if fixed: flags += ['-DTAB5_MESH_KEYBOARD_FIXED']
    cmd = ['g++',*flags,str(HERE/'backend-ui.cpp'),*[str(sketch/name) for name in
            ['Tab5Focus.cpp','Tab5TextInput.cpp','Tab5UiStyle.cpp','WriterDocument.cpp']],
            str(lvgl_build/'liblvgl.a'),'-pthread','-o',str(work/'test-mesh-keyboard')]
    result = subprocess.run(cmd,capture_output=True,text=True)
    (evidence/'compile.log').write_text(result.stdout+result.stderr)
    if result.returncode: print(result.stdout+result.stderr); raise SystemExit(result.returncode)
    env = dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    result = subprocess.run([str(work/'test-mesh-keyboard'),str(evidence),str(sketch/'sdcard/System/font.ttf')],
                            capture_output=True,text=True,env=env,timeout=120)
    (evidence/'run.log').write_text(result.stdout+result.stderr)
    print(result.stdout+result.stderr)
    if result.returncode: raise SystemExit(result.returncode)
    assert hashes == {name:sha(sketch/name) for name in names}, 'Production changed during tests'
    assert hook == next(line.strip() for line in ino.read_text().splitlines() if line.strip().startswith('meshHooks.keyboard='))
    from PIL import Image
    for ppm in evidence.glob('*.ppm'): Image.open(ppm).save(ppm.with_suffix('.png')); ppm.unlink()
    return {'passed':True, 'result':json.loads(result.stdout.strip().splitlines()[-1]),
            'sourceHashes':hashes,'keyboardHook':hook,'keyboardHookSha256':hashlib.sha256(hook.encode()).hexdigest(),
            'compileCommand':cmd}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',type=Path,required=True)
    parser.add_argument('--lvgl',type=Path,required=True)
    parser.add_argument('--lvgl-build',type=Path,required=True)
    parser.add_argument('--build',type=Path,required=True)
    args=parser.parse_args()
    sketch=ROOT/'Tab5OS_Arduino_v0.8.53_Ecriture'
    test_hashes={p.relative_to(ROOT).as_posix():sha(p) for p in HERE.iterdir() if p.is_file()}
    evidence=ROOT/'tools/verification/mesh-keyboard-0853'
    results={}
    for mode,source,fixed in [('baseline',args.baseline.resolve(),False),('current',sketch,True)]:
        results[mode]=execute(source,args.build.resolve()/mode,evidence/mode,args.lvgl.resolve(),args.lvgl_build.resolve(),fixed)
    assert test_hashes == {p.relative_to(ROOT).as_posix():sha(p) for p in HERE.iterdir() if p.is_file()}
    report={'version':'0.8.53','passed':True,'scope':__doc__,'physicalDeviceTested':False,
            'sanitizers':['address','undefined'],'results':results,
            'sourceHashes':{sketch.name+'/'+name:h for name,h in results['current']['sourceHashes'].items()},
            'testSourceHashes':test_hashes,
            'evidenceHashes':{p.relative_to(ROOT).as_posix():sha(p) for p in evidence.rglob('*') if p.is_file()},
            'lvglArchiveSha256':sha(args.lvgl_build/'liblvgl.a'),
            'integrationSnippet':{'file':sketch.name+'/'+sketch.name+'.ino','text':results['current']['keyboardHook'],
                                  'sha256':results['current']['keyboardHookSha256']},
            'limitations':['No physical USB, touch, C6 Bluetooth, radio or scheduler test.',
                           'Global keyboard rendering/visibility adapter is a host boundary; production Mesh UI, key handler, pairing/send callbacks and OS hook are real.',
                           'ASan/UBSan do not establish whole-OS leak freedom; explicit application allocation counters are checked after closure.']}
    (ROOT/'tools/verification/mesh-keyboard-0853.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'passed':True,'baseline':results['baseline']['result'],'current':results['current']['result']}))
if __name__=='__main__':main()
