#!/usr/bin/env python3
"""Actual manager and allocator helper: compare .41 walks and test .42 safety."""
import argparse,hashlib,json,os,pathlib,subprocess,tempfile
HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SKETCH=ROOT/'Tab5OS_Arduino_v0.8.42_Ecriture'
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path);args=p.parse_args()
build=args.build or pathlib.Path(tempfile.mkdtemp(prefix='tab5-memory-0842-'))
build.mkdir(parents=True,exist_ok=True)
evidence=ROOT/'tools/verification/memory-0842';evidence.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
common=['g++','-std=c++17','-O1','-g','-pthread','-fno-omit-frame-pointer','-fsanitize=address,undefined','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function']
commands=[]
def run(name,sources,includes,extra=()):
 command=common+list(extra)
 for include in includes:command+=['-I',str(include)]
 command+=list(map(str,sources))+['-o',str(build/('test-'+name))];commands.append(command)
 r=subprocess.run(command,text=True,capture_output=True);(evidence/(name+'-build.log')).write_text(r.stdout+r.stderr);r.check_returncode()
 r=subprocess.run([str(build/('test-'+name))],env=env,text=True,capture_output=True);(evidence/(name+'-run.log')).write_text(r.stdout+r.stderr);r.check_returncode();return json.loads(r.stdout)
baseline=run('baseline',[HERE/'admission.cpp',HERE/'baseline-0841/Tab5ResourceManager.cpp'],[HERE/'baseline-0841',SKETCH],['-DTAB5_TEST_FIXED=0'])
fixed=run('fixed',[HERE/'admission.cpp',SKETCH/'Tab5ResourceManager.cpp'],[SKETCH],['-DTAB5_TEST_FIXED=1'])
regression=run('regression',[HERE/'regression.cpp',SKETCH/'Tab5ResourceManager.cpp'],[SKETCH])
sampler=run('sampler',[HERE/'sampler.cpp',SKETCH/'Tab5Memory.cpp',SKETCH/'Tab5MemoryLayout.cpp'],[HERE/'stubs',SKETCH])
assert fixed['stableInternalDeficitHeapWalks']<baseline['stableInternalDeficitHeapWalks']
sources=[SKETCH/n for n in ['Tab5ResourceManager.cpp','Tab5ResourceManager.h','Tab5ResourcePolicy.h','Tab5Memory.cpp','Tab5Memory.h','Tab5MemoryLayout.cpp','Tab5MemoryLayout.h']]
report={'version':'0.8.42','passed':True,'baseline':baseline,'fixed':fixed,'regression':regression,'sampler':sampler,'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],'scope':['Real .41 versus .42 manager with identical stable byte-deficit heap model','Final refusal retains exact full sample, safety margins, reason and one denial count','Pending quotas, deferred retired stack return, UI collector, worker timeout and fragmentation/DMA/PSRAM refusal','All eight block-capability combinations in actual Tab5Memory.cpp','Explicit diagnostic formatter with total/free/minimum/largest and physical P4 split; null output causes no scans','Historical ownership, reentrant collection, concurrent recovery and 400 repeated lifecycle cycles'],'limits':['No target time, IRQ latency or physical free SRAM measured','Heap capacities simulated; all admission and sampler production code compiled unmodified','Does not establish that admission explains continuous idle mouse lag','LeakSanitizer unavailable in this host: ptrace/proc task access restriction; ASan and UBSan remain active'],'sourceHashes':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sources},'testHashes':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in HERE.rglob('*') if f.is_file()},'commands':commands}
(ROOT/'tools/verification/memory-0842.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ['passed','baseline','fixed','regression','sampler']},indent=2))
