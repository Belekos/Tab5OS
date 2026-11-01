from pathlib import Path
import argparse,hashlib,json,os,subprocess,tempfile
here=Path(__file__).resolve().parent
release=here.parent.parent
parser=argparse.ArgumentParser();parser.add_argument('--sketch',type=Path);args=parser.parse_args()
sketch=args.sketch or release/'Tab5OS_Arduino_v0.8.30_Ecriture'
inputs=[sketch/p for p in ['Tab5Git.cpp','Tab5Git.h','Tab5GitMemory.h','Tab5GitView.cpp','Tab5ResourceManager.cpp','Tab5ResourceManager.h','Tab5ResourcePolicy.h','Tab5Task.h']]+[here/'run.py',here/'memory.cpp']
def hashes():return {p.relative_to(release).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
original=hashes();source=(sketch/'Tab5Git.cpp').read_text()
allocator=source[source.index('std::atomic_flag active='):source.index('template<class T,void(*Free)')]
startup=next(line for line in source.splitlines() if 'auto budget=networkOperation?' in line)
assert 'MemoryLimit+' not in startup
assert 'std::max' in startup
assert 'tab5CreateTask(worker,"tab5_git_ui",Tab5Git::WorkerStackBytes,nullptr,1,nullptr,0,&admission)' in (sketch/'Tab5GitView.cpp').read_text()
assert 'tab5CreateTask(worker,"tab5_git_terminal",WorkerStackBytes,&job,1,nullptr,0,&admission)' in source
with tempfile.TemporaryDirectory(prefix='tab5-git-0830-') as tmp:
 work=Path(tmp);(work/'allocator.inc').write_text(allocator);(work/'startup.inc').write_text(startup)
 command=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-pthread','-I'+str(sketch),'-I'+str(work),str(here/'memory.cpp'),str(sketch/'Tab5ResourceManager.cpp'),'-o',str(work/'memory')]
 subprocess.run(command,check=True)
 run=subprocess.run([str(work/'memory')],check=True,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1'))
 result=json.loads(run.stdout)
assert original==hashes(),'Sources changed during test'
report={'version':'0.8.30','passed':True,'physicalDeviceTested':False,'method':'Exact production Git allocation/reallocation/free block and startup budget extracted byte-for-byte; actual AllocationBudget and ResourceManager compiled with ASan/UBSan. Simulated capability heap (PSRAM/SRAM/fragmentation), real host malloc backing buffers. No libgit2 transport, actual TLS, RTOS scheduling or device clone exercised.','regressions':['Previous unconditional 8-MiB reservation refuses 4-MiB free; actual small working set is now admitted','Progressive 256-KiB batches with exact-size fallback retain global reserves and respect concurrent app reservations','Total tracked quota includes headers and transient realloc blocks','Failure/cancellation/400 repeated sessions release physical heap and reservation slots','40-KiB stack requirement, internal TLS/external TLS headroom, fragmentation and diagnostic classification preserved'],'result':result,'sanitizers':['address','undefined'],'leakSanitizer':False,'sourceHashes':original,'extractedHashes':{'allocator':hashlib.sha256(allocator.encode()).hexdigest(),'startupBudget':hashlib.sha256(startup.encode()).hexdigest()}}
out=release/'tools/verification/git-0830.json';out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'passed':True,'checks':result['checks'],'report':str(out)}))
