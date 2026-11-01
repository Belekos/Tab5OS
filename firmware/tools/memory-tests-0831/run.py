#!/usr/bin/env python3
"""Exercise the actual resource manager with controlled capability heaps."""
import argparse, hashlib, json, pathlib, subprocess, tempfile
HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parents[1]
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path);args=p.parse_args()
SKETCH=ROOT/'Tab5OS_Arduino_v0.8.31_Ecriture'
build=args.build or pathlib.Path(tempfile.mkdtemp(prefix='tab5-memory-0831-'));build.mkdir(parents=True,exist_ok=True)
exe=build/'memory-tests'
command=['g++','-std=c++17','-g','-O1','-pthread','-fno-omit-frame-pointer','-fsanitize=address,undefined','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I',str(SKETCH),str(HERE/'main.cpp'),str(SKETCH/'Tab5ResourceManager.cpp'),'-o',str(exe)]
subprocess.run(command,check=True)
import os
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
r=subprocess.run([str(exe)],env=env,text=True,capture_output=True)
(build/'run.log').write_text(r.stdout+r.stderr)
print(r.stdout,end='');print(r.stderr,end='');r.check_returncode()
result=json.loads(r.stdout)
files=[HERE/'main.cpp',HERE/'run.py',SKETCH/'Tab5ResourceManager.h',SKETCH/'Tab5ResourceManager.cpp',SKETCH/'Tab5ResourcePolicy.h']
report={**result,'version':'0.8.31','sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],'scope':['Actual production ResourceManager; simulated internal/PSRAM capabilities','Exact 46KiB/48KiB diagnostic; same32KiB/512KiB safety margins','Capability-specific eviction, fragmentation/DMA, active callback veto, reentrant collector','Idle expiry and foreground return, one callback per second, millis rollover','Concurrent worker collection request, timeout cleanup, deferred retired-stack release','400 repeated reserve/release/move cycles'],'limits':['No physical Tab5 heap, FreeRTOS or Wi-Fi/network operation','Cache data/liveness represented by controlled callbacks; app callbacks covered separately','LeakSanitizer disabled in host environment; ASan and UBSan active'],'sourceHashes':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'compileCommand':command}
registration=(SKETCH/(SKETCH.name+'.ino')).read_text().split('static void registerResourceCaches() {',1)[1].split('\nvoid setup()',1)[0]
report['integrationRegistrationSha256']=hashlib.sha256(registration.encode()).hexdigest()
out=ROOT/'tools/verification/memory-0831.json';out.write_text(json.dumps(report,indent=2)+'\n');print(out)
