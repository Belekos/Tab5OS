#!/usr/bin/env python3
"""Execute actual .ino foreground routing/cache registration with controlled app boundaries."""
import argparse, hashlib, json, os, pathlib, subprocess, tempfile
HERE=pathlib.Path(__file__).resolve().parent;ROOT=HERE.parents[1];SKETCH=ROOT/'Tab5OS_Arduino_v0.8.33_Ecriture'
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path);args=p.parse_args()
build=args.build or pathlib.Path(tempfile.mkdtemp(prefix='tab5-memory-integration-0833-'));build.mkdir(parents=True,exist_ok=True)
ino=SKETCH/(SKETCH.name+'.ino');source=ino.read_text()
def body(signature):
    begin=source.index(signature+' {');start=source.index('{',begin);depth=1;end=start+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[begin:end]
extracted=body('static void updateResourceForeground()')+'\n'+body('static void registerResourceCaches()')+'\n'
(build/'resource_integration.inc').write_text(extracted)
exe=build/'integration';command=['g++','-std=c++17','-g','-O1','-pthread','-fno-omit-frame-pointer','-fsanitize=address,undefined','-Wall','-Wextra','-Werror','-I',str(SKETCH),'-I',str(build),str(HERE/'integration.cpp'),str(SKETCH/'Tab5ResourceManager.cpp'),'-o',str(exe)]
subprocess.run(command,check=True);exe.chmod(exe.stat().st_mode|0o111)
r=subprocess.run([str(exe)],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),capture_output=True,text=True)
(build/'run.log').write_text(r.stdout+r.stderr);print(r.stdout,r.stderr,end='');r.check_returncode()
files=[HERE/'integration.cpp',HERE/'run_integration.py',SKETCH/'Tab5ResourceManager.cpp',SKETCH/'Tab5ResourceManager.h',SKETCH/'Tab5ResourcePolicy.h',ino]
report={**json.loads(r.stdout),'version':'0.8.33','sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],'scope':['Verbatim .ino updateResourceForeground and registerResourceCaches compiled with real manager','11 routing scenarios including native image editor, Tools overlays, reader and video','9 callback scenarios including visible Maps/Explorer/media underneath Tools overlay,printing/JPEG and hidden cache release'],'limits':['App methods,LVGL deletion,storage handles and heaps represented by controlled doubles','Does not execute native image editor UI or real display'],'sourceHashes':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'extractedIntegrationSha256':hashlib.sha256(extracted.encode()).hexdigest(),'compileCommand':command}
path=ROOT/'tools/verification/memory-integration-0833.json';path.write_text(json.dumps(report,indent=2)+'\n');print(path)
