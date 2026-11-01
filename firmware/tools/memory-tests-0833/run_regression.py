#!/usr/bin/env python3
"""Run identical event scenarios against shipped0.8.32 and corrected0.8.33."""
import argparse, hashlib, json, os, pathlib, subprocess, tempfile
HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SKETCH=ROOT/'Tab5OS_Arduino_v0.8.33_Ecriture'
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path);args=p.parse_args()
build=args.build or pathlib.Path(tempfile.mkdtemp(prefix='tab5-memory-regression-0833-'));build.mkdir(parents=True,exist_ok=True)
def body(source, signature):
    begin=source.index(signature);start=source.index('{',begin);depth=1;end=start+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[begin:end]
results={};commands={}
for name, baseline in [('baseline',True),('corrected',False)]:
    source_dir=HERE/'baseline-0832' if baseline else SKETCH
    source=(source_dir/'Tab5ResourceManager.cpp').read_text()
    out=build/name;out.mkdir(exist_ok=True)
    (out/'resource_service.inc').write_text(body(source,'void allocationFailed(')+'\n'+body(source,'void tab5ResourcesService(')+'\n')
    exe=out/'regression'
    command=['g++','-std=c++17','-g','-O1','-pthread','-fno-omit-frame-pointer','-fsanitize=address,undefined','-Wall','-Wextra','-Werror','-Wno-unused-function','-I',str(source_dir),'-I',str(SKETCH),'-I',str(out),str(HERE/'regression.cpp'),str(source_dir/'Tab5ResourceManager.cpp'),'-o',str(exe)]
    if baseline:command.append('-DBASELINE_0832=1')
    subprocess.run(command,check=True);exe.chmod(exe.stat().st_mode|0o111)
    result=subprocess.run([str(exe)],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),capture_output=True,text=True)
    (out/'run.log').write_text(result.stdout+result.stderr)
    print(name,result.stdout,result.stderr,end='');result.check_returncode()
    results[name]=json.loads(result.stdout);commands[name]=command
bad_events=['idleWithoutDemand','lowButIdle','foregroundEvicted','requesterEvicted','staleFailureEvicted']
for event in bad_events:
    assert results['baseline'][event]==1,(event,results)
    assert results['corrected'][event]==0,(event,results)
assert results['baseline']['failureDiagnostic']==results['corrected']['failureDiagnostic']==1
files=[HERE/'regression.cpp',HERE/'run_regression.py',SKETCH/'Tab5ResourceManager.h',SKETCH/'Tab5ResourceManager.cpp',SKETCH/'Tab5ResourcePolicy.h',*sorted((HERE/'baseline-0832').glob('*'))]
report={'version':'0.8.33','passed':True,'checks':12,'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],'results':results,'scope':['Exact shipped0.8.32 resource-manager sources retained unmodified as regression fixtures','Identical simulated navigation,500msmaintenance,30sidleexpiry,foreground and requesting-owner demand scenarios','Verbatim Arduino allocationFailed and tab5ResourcesService bodies with simulated hardware boundaries','Optional PSRAM failure already handled by its caller does not later purge an unrelated SRAMworker'],'limits':['Simulated capability heaps and time, no physical Tab5 display or allocators','Does not establish physical flicker origin or frame rate'],'sourceHashes':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'compileCommands':commands}
path=ROOT/'tools/verification/memory-regression-0833.json';path.write_text(json.dumps(report,indent=2)+'\n');print(path)
