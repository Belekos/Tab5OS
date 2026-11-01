#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--baseline',required=True,type=Path)
    parser.add_argument('--out',type=Path)
    args=parser.parse_args()
    tests=Path(__file__).resolve().parent
    release=tests.parent.parent
    source=release/'Tab5OS_Arduino_v0.8.47_Ecriture'
    out=args.out or release/'tools/verification/media-persistence-0847'
    out.mkdir(parents=True,exist_ok=True)
    assert sha(args.baseline)=='22d99628cc3ab6a9673be1886755ec9a3092aaede3f017a7e0e20443cc0816a6'
    result={'production':['Tab5MediaSession.cpp','Tab5MediaSession.h'],'boundaries':['FreeRTOS task creation with real host threads','NVS key storage/failure/blocking','heap capability allocation','DSI counter snapshot','monotonic timer'],'hardwareTest':False,'sanitizers':['ASan','UBSan'],'leakSanitizer':False,'sources':{name:sha(source/name) for name in ['Tab5MediaSession.cpp','Tab5MediaSession.h']}}
    for variant,current in [('baseline',False),('current',True)]:
        build=out/variant;build.mkdir(exist_ok=True)
        for p in tests.glob('*.h'):shutil.copyfile(p,build/p.name)
        shutil.copyfile(tests/'main.cpp',build/'main.cpp')
        for name in result['production']:
            if current:shutil.copyfile(source/name,build/name)
            else:
                with zipfile.ZipFile(args.baseline) as z:
                    (build/name).write_bytes(z.read('Tab5OS_Ecriture_v0.8.46/Tab5OS_Arduino_v0.8.46_Ecriture/'+name))
        command=['g++','-std=c++17','-g','-O1','-pthread','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-DCURRENT='+str(int(current)),str(build/'main.cpp'),str(build/'Tab5MediaSession.cpp'),'-I'+str(build),'-o',str(build/'test')]
        compiled=subprocess.run(command,text=True,capture_output=True)
        (build/'compile.log').write_text(compiled.stdout+compiled.stderr)
        if compiled.returncode:raise RuntimeError(f'{variant} compile failed: {compiled.stderr}')
        env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0:abort_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
        run=subprocess.run([str(build/'test'),str(build)],env=env,text=True,capture_output=True,timeout=60)
        (build/'run.log').write_text(run.stdout+run.stderr)
        if run.returncode:raise RuntimeError(f'{variant} tests failed: {run.stderr}')
        result[variant]=json.loads(run.stdout.strip())
        result[variant]['recordSha256']=sha(build/'record.bin')
    result['byteCompatibleVersion1']=result['baseline']['recordSha256']==result['current']['recordSha256']
    result['sourcesUnchanged']=all(sha(source/name)==digest for name,digest in result['sources'].items())
    result['passed']=result['byteCompatibleVersion1'] and result['sourcesUnchanged'] and all(result[k]['passed'] for k in ['baseline','current'])
    report=out.parent/'media-persistence-0847.json';report.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))
    return 0 if result['passed'] else 1

if __name__=='__main__':raise SystemExit(main())
