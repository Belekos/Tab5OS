#!/usr/bin/env python3
"""Compile actual manager core, exercising native synchronization at RTOS boundaries."""
import argparse,hashlib,json,os,pathlib,subprocess,tempfile
HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SKETCH=ROOT/'Tab5OS_Arduino_v0.8.47_Ecriture'
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path);args=p.parse_args()
build=args.build or pathlib.Path(tempfile.mkdtemp(prefix='tab5-memory-0847-'))
build.mkdir(parents=True,exist_ok=True)
evidence=ROOT/'tools/verification/memory-0847';evidence.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
common=['g++','-std=c++17','-O1','-g','-pthread','-fno-omit-frame-pointer','-fsanitize=address,undefined','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function']
commands=[]
def run(name,sources,includes,extra=()):
 command=common+list(extra)
 for include in includes:command+=['-I',str(include)]
 command+=list(map(str,sources))+['-o',str(build/('test-'+name))];commands.append(command)
 r=subprocess.run(command,text=True,capture_output=True);(evidence/(name+'-build.log')).write_text(r.stdout+r.stderr);r.check_returncode()
 r=subprocess.run([str(build/('test-'+name))],env=env,text=True,capture_output=True);(evidence/(name+'-run.log')).write_text(r.stdout+r.stderr);r.check_returncode();return json.loads(r.stdout)
# Keep the exact production namespace including radio-budget helpers and all
# allocation/admission/ownership logic. Only global Arduino bootstrap wrappers
# are excluded from the host boundary test; the full target build covers them.
for name,source in [('current',SKETCH/'Tab5ResourceManager.cpp'),('baseline',HERE/'baseline-0846/Tab5ResourceManager.cpp')]:
 text=source.read_text();marker='\n#ifdef ARDUINO\n#include "Tab5Memory.h"'
 assert text.count(marker)==1
 (build/(name+'-core.cpp')).write_text(text.split(marker)[0])
baseline=run('baseline-native',[HERE/'native.cpp',build/'baseline-core.cpp'],[HERE/'stubs',HERE/'baseline-0846',SKETCH],['-DARDUINO','-DTAB5_TEST_FIXED=0'])
fixed=run('current-native',[HERE/'native.cpp',build/'current-core.cpp'],[HERE/'stubs',SKETCH],['-DARDUINO','-DTAB5_TEST_FIXED=1'])
legacy=ROOT/'tools/memory-tests-0842'
admission=run('admission',[legacy/'admission.cpp',SKETCH/'Tab5ResourceManager.cpp'],[SKETCH],['-DTAB5_TEST_FIXED=1'])
regression=run('regression',[legacy/'regression.cpp',SKETCH/'Tab5ResourceManager.cpp'],[SKETCH])
assert fixed['pendingCollectorTickWakeups']==0<baseline['pendingCollectorTickWakeups']
sources=[SKETCH/n for n in ['Tab5ResourceManager.cpp','Tab5ResourceManager.h','Tab5ResourcePolicy.h']]
report={'version':'0.8.47','passed':True,'baseline':baseline,'fixed':fixed,'admission':admission,'regression':regression,'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],'scope':['Actual .46 versus .47 admission/collection core, native ARDUINO path','FreeRTOS mutex and event group APIs adapted to condition variables at host boundary; event bits and per-request clearing retained','Pending collection sleeps, completion before wait, eight simultaneous requests, timeout without collector','Timeout while callback owns a request; late completion retains slot ownership until safe reuse','Native lock contention is bounded and reports AdmissionBusy, no tick-poll loop','Historical safety margins, final exact refusal, byte/capability checks, quotas and400 lifecycle cycles'],'limits':['No target scheduling latency, priority inheritance timing or free memory measured','Priority inheritance is provided by target FreeRTOS mutex; host timed_mutex does not simulate scheduler priorities','Host native-core extraction excludes global bootstrap wrapper; complete target build required','UI recovering admission and post-collection deferred stack return still have bounded tick observation; not declared fully asynchronous','LeakSanitizer unavailable in environment; ASan and UBSan enabled'],'sourceHashes':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sources},'testHashes':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in HERE.rglob('*') if f.is_file()},'commands':commands}
(ROOT/'tools/verification/memory-0847.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ['passed','baseline','fixed','admission','regression']},indent=2))
