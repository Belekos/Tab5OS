from pathlib import Path
import argparse,hashlib,json,os,subprocess,tempfile
here=Path(__file__).resolve().parent
release=here.parent.parent
parser=argparse.ArgumentParser();parser.add_argument('--sketch',type=Path);args=parser.parse_args()
sketch=args.sketch or release/'Tab5OS_Arduino_v0.8.32_Ecriture'
inputs=[sketch/p for p in ['Tab5Git.cpp','Tab5Git.h','Tab5GitMemory.h','Tab5GitView.cpp','Tab5ResourceManager.cpp','Tab5ResourceManager.h','Tab5ResourcePolicy.h','Tab5Task.h']]+[here/'run_memory.py',here/'memory.cpp']
def hashes():return {p.relative_to(release).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
original=hashes();source=(sketch/'Tab5Git.cpp').read_text()
allocator=source[source.index('std::atomic_flag active='):source.index('template<class T,void(*Free)')]
startup=(sketch/'Tab5GitMemory.h').read_text()
view=(sketch/'Tab5GitView.cpp').read_text()
worker=view[view.index('void worker(void*){'):view.index('void panel(lv_obj_t* o)')]
handoff=source[source.index(' auto admitted=startupAdmission?'):source.index('#else',source.index(' auto admitted=startupAdmission?'))]
assert 'startupAdmission&&*startupAdmission?std::move(*startupAdmission)' in source
assert 'auto admitted=startupAdmission?std::move(*startupAdmission)' in source
assert view.index('model->startup=Tab5Git::reserveStartup(args,&admission)') < view.index('tab5CreateTask(worker,"tab5_git_ui"')
assert source.index('job.startup=reserveStartup(argv,&admission)') < source.index('tab5CreateTask(worker,"tab5_git_terminal"')
assert 'if(tab5TlsMemoryExternal())return {8*KiB,4*KiB,0,256*KiB,64*KiB};' in (sketch/'Tab5ResourceManager.cpp').read_text()
assert 'tab5CreateTask(worker,"tab5_git_ui",Tab5Git::WorkerStackBytes,nullptr,1,nullptr,0,&admission)' in (sketch/'Tab5GitView.cpp').read_text()
assert 'tab5CreateTask(worker,"tab5_git_terminal",WorkerStackBytes,&job,1,nullptr,0,&admission)' in source
with tempfile.TemporaryDirectory(prefix='tab5-git-0832-') as tmp:
 work=Path(tmp);(work/'allocator.inc').write_text(allocator);(work/'worker.inc').write_text(worker);(work/'handoff.inc').write_text(handoff)
 command=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-pthread','-I'+str(sketch),'-I'+str(work),str(here/'memory.cpp'),str(sketch/'Tab5ResourceManager.cpp'),'-o',str(work/'memory')]
 subprocess.run(command,check=True)
 run=subprocess.run([str(work/'memory')],check=True,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1'))
 result=json.loads(run.stdout)
assert original==hashes(),'Sources changed during test'
report={'version':'0.8.32','passed':True,'physicalDeviceTested':False,'method':'Exact production Git allocation/reallocation/free block and UI worker extracted byte-for-byte; actual startupBudget, reserveStartup, AllocationBudget and ResourceManager compiled with ASan/UBSan. Simulated capability heap (PSRAM/SRAM/fragmentation), real host malloc backing buffers. No libgit2 transport, actual TLS, RTOS scheduling or device clone exercised.','regressions':['Reported 46-KiB free / 48-KiB session requirement reproduced after simulated40-KiB worker stack; session reservation retained before stack launch triggers collection for full combined demand', 'Admitted startup survives worker handoff without re-admission or double charge; early failures return both reservations', 'Failed Clone/New no longer executes unrelated git status or appends not-a-repository diagnostic','Previous unconditional 8-MiB reservation refuses 4-MiB free; actual small working set is now admitted','Progressive 256-KiB batches with exact-size fallback retain global reserves and respect concurrent app reservations','Total tracked quota includes headers and transient realloc blocks','Failure/cancellation/400 repeated sessions release physical heap and reservation slots','40-KiB stack requirement, internal TLS/external TLS headroom, fragmentation and diagnostic classification preserved'],'result':result,'sanitizers':['address','undefined'],'leakSanitizer':False,'sourceHashes':original,'extractedHashes':{'allocator':hashlib.sha256(allocator.encode()).hexdigest(),'startupBudgetHeader':hashlib.sha256(startup.encode()).hexdigest(),'uiWorker':hashlib.sha256(worker.encode()).hexdigest(),'startupHandoff':hashlib.sha256(handoff.encode()).hexdigest()}}
out=release/'tools/verification/git-memory-0832.json';out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'passed':True,'checks':result['checks'],'report':str(out)}))
