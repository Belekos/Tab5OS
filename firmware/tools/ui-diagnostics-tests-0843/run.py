#!/usr/bin/env python3
"""Verify actual UI profiler/journal with deterministic SDK boundaries."""
import argparse,hashlib,json,os,pathlib,subprocess,tempfile
HERE=pathlib.Path(__file__).resolve().parent;ROOT=HERE.parents[1]
SKETCH=ROOT/'Tab5OS_Arduino_v0.8.43_Ecriture'
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path);args=p.parse_args()
build=args.build or pathlib.Path(tempfile.mkdtemp(prefix='tab5-ui-diagnostics-0843-'));build.mkdir(parents=True,exist_ok=True)
evidence=ROOT/'tools/verification/ui-diagnostics-0843';evidence.mkdir(parents=True,exist_ok=True)
command=['g++','-std=c++17','-O1','-g','-fno-omit-frame-pointer','-fsanitize=address,undefined','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I',str(HERE/'stubs'),'-I',str(SKETCH),str(HERE/'diagnostics.cpp'),'-o',str(build/'test-ui-diagnostics')]
r=subprocess.run(command,text=True,capture_output=True);(evidence/'build.log').write_text(r.stdout+r.stderr);r.check_returncode()
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
r=subprocess.run([str(build/'test-ui-diagnostics')],env=env,text=True,capture_output=True);(evidence/'run.log').write_text(r.stdout+r.stderr);r.check_returncode();result=json.loads(r.stdout)
sources=[SKETCH/n for n in ['Tab5UiDiagnostics.cpp','Tab5UiDiagnostics.h','Tab5DisplayActivity.h']]
report={'version':'0.8.43',**result,'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],'sourceHashes':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sources},'testSourceHashes':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in HERE.rglob('*') if f.is_file()},'command':command,'scope':['Disabled path reads no clock and allocates no memory; existing display attribution maintained', 'One PSRAM-only state on first activation, two injected allocation failures with successful retry, retained counters on disable, reset releases state, no internal fallback','20 ms inclusive threshold, per-phase one-second journal limit, all slow counts retained','Scope/loop enable generations; active time separated from inter-loop gap','Eight-entry circular history, full-line bounded UTF-8 formatting including tiny buffers','All accepted warm resets, cold/brownout/unknown discard, magic/count/index/phase/checksum corruption','64-bit accumulated time and saturation of 32-bit maxima'],'limits':['Clock/reset/device observer are stubs; production profiler and journal compiled unchanged','Retained bytes across actual hardware reset not exercised','Wall duration includes preemption and wait; not CPU utilization','LeakSanitizer unavailable under host ptrace/proc restrictions; ASan/UBSan active']}
(ROOT/'tools/verification/ui-diagnostics-0843.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(result,indent=2))
