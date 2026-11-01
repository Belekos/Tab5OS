#!/usr/bin/env python3
"""Whole production catalogue, before/after; only filesystem/RTOS/UI edges are doubles."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
r=Path(__file__).resolve().parents[2];s=r/'Tab5OS_Arduino_v0.8.43_Ecriture';t=Path(__file__).resolve().parent
o=r/'tools/verification/catalogue-0843';o.mkdir(parents=True,exist_ok=True)
sources=[s/n for n in ['Tab5AppCatalogue.cpp','Tab5AppCatalogue.h','Tab5AppCataloguePolicy.h','Tab5Executable.cpp','Tab5PythonServices.h','Tab5Task.h','Tab5ResourceManager.h']]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
before={str(p.relative_to(r)):sha(p) for p in sources};results={}
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
for baseline in [True,False]:
 label='baseline' if baseline else 'corrected'
 with tempfile.TemporaryDirectory(prefix='tab5-catalogue-0843-') as tmp:
  exe=Path(tmp)/'apps';cmd=['g++','-std=c++17','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(t/'mocks')]
  if baseline:cmd+=['-DCATALOGUE_BASELINE','-I'+str(t/'baseline-0842')]
  cmd+=['-I'+str(s),str(t/'test_app_scan.cpp'),'-o',str(exe)]
  b=subprocess.run(cmd,capture_output=True,text=True);(o/(label+'-build.log')).write_text(b.stdout+b.stderr);b.check_returncode()
  run=subprocess.run([str(exe)],capture_output=True,text=True,env=env);(o/(label+'-run.log')).write_text(run.stdout+run.stderr)
  print(run.stdout+run.stderr,end='');run.check_returncode();results[label]=dict(passed=True,output=run.stdout.strip())
  cmd=['python3',str(t/'test_elf.py'),'--sketch',str(s),'--loader','--report',str(o/(label+'-elf.json'))]
  if baseline:cmd+=['--baseline']
  run=subprocess.run(cmd,capture_output=True,text=True,env=env);(o/(label+'-elf.log')).write_text(run.stdout+run.stderr)
  print(run.stdout+run.stderr,end='');run.check_returncode()
assert before=={str(p.relative_to(r)):sha(p) for p in sources},'Production source changed during tests'
with tempfile.TemporaryDirectory(prefix='tab5-catalogue-worker-0843-') as tmp:
 exe=Path(tmp)/'worker'
 cmd=['g++','-std=c++17','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-pthread','-I'+str(t/'mocks'),'-I'+str(t),'-I'+str(s),str(t/'test_worker.cpp'),'-o',str(exe)]
 b=subprocess.run(cmd,capture_output=True,text=True);(o/'worker-build.log').write_text(b.stdout+b.stderr);b.check_returncode()
 run=subprocess.run([str(exe)],capture_output=True,text=True,env=env);(o/'worker-run.log').write_text(run.stdout+run.stderr)
 print(run.stdout+run.stderr,end='');run.check_returncode();results['worker']=dict(passed=True,output=run.stdout.strip())
assert before=={str(p.relative_to(r)):sha(p) for p in sources},'Production source changed during worker tests'
report=dict(version='0.8.43',passed=True,physicalDeviceTested=False,sourceHashes=before,
 testSourceHashes={str(p.relative_to(r)):sha(p) for p in t.rglob('*') if p.is_file() and '__pycache__' not in str(p)},
 evidenceHashes={str(p.relative_to(r)):sha(p) for p in o.iterdir() if p.is_file()},results=results,
 method='Actual full catalogue implementation and actual ELF loader functions; ASan/UBSan. Baseline code copied unchanged from delivered 0.8.42. FS, NVS, relocation/RTOS and LVGL icon boundary adapters are doubles.',
 scenarios=['Seven ordinary ELF names/locations formerly omitted; now all discovered','Four launcher filename blacklists removed; request accepted, SD remains deferred, cancellation cleans up','Explicit hidden/pinned choice remains hidden across scans','Private Python modules and dotfiles remain omitted','Added/deleted/unchanged/reordered directory contents; atomic publication; cancellation, EIO and staging OOM','Optional entry read EIO preserves previous catalogue instead of publishing deletion','Real host worker: pre-entry cancellation without any I/O; blocked I/O with 1000 UI polls; latest queued restart; reset during scan; SD withdrawal; 100 task/handle/lease cycles; task creation failure falls back' ,'64-app bound, SD-only and USB-only mount handling, 100 repeat scans without new table allocations'],
 limitations=['No on-device SD timing, physical touch/scroll or ELF execution is measured.','Firmware still has bounded 64-app/384-entry catalogue capacity.','LSan is disabled under executor ptrace; tests track owned files, allocation outcomes and retained tables explicitly.'])
(o.parent/'catalogue-0843.json').write_text(json.dumps(report,indent=2)+'\n')
