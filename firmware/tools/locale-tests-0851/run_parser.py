#!/usr/bin/env python3
"""Differential language-pack parser checks and a host-only benchmark."""
from pathlib import Path
import argparse, hashlib, json, os, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
import sys
sys.path.insert(0,str(ROOT/'tools'))
from release_config import SKETCH as SKETCH_NAME, VERSION, TAG
SKETCH=ROOT/SKETCH_NAME
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--arduino-json',type=Path,default=ROOT.parent/'build_0850/sketchbook/libraries/ArduinoJson/src')
p.add_argument('--build',type=Path,default=ROOT.parent/'build_0851/locale-parser')
a=p.parse_args();build=a.build.resolve();build.mkdir(parents=True,exist_ok=True)
proof=ROOT/'tools/verification/locale-parser-0851';proof.mkdir(parents=True,exist_ok=True)
files=[SKETCH/'Tab5LanguageReader.h',SKETCH/'Tab5TranslationFormat.h',HERE/'parser_test.cpp',Path(__file__)]
files+=sorted((SKETCH/'sdcard/System/languages').glob('*.json'))
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
before={str(p.relative_to(ROOT)):sha(p) for p in files}
common=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-error=maybe-uninitialized','-I'+str(SKETCH),'-isystem',str(a.arduino_json.resolve()),str(HERE/'parser_test.cpp')]
def run(cmd,name,env=None):
 r=subprocess.run([str(c) for c in cmd],capture_output=True,text=True,env=env,timeout=180)
 (proof/name).write_text(r.stdout+r.stderr)
 if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
 return r.stdout
run([*common,'-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-o',build/'parser-asan'],'compile-asan.log')
output=run([build/'parser-asan',SKETCH/'sdcard/System/languages'],'tests.log',dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
print(output,end='');tests=json.loads(output.strip().splitlines()[-1])
run([*common,'-O2','-DNDEBUG','-o',build/'parser-release'],'compile-release.log')
output=run([build/'parser-release','--benchmark',SKETCH/'sdcard/System/languages'],'benchmark.log')
print(output,end='');benchmark=json.loads(output.strip().splitlines()[-1])
assert before=={str(p.relative_to(ROOT)):sha(p) for p in files},'Source changed while testing'
report={'version':VERSION,'passed':True,'physicalDeviceTested':False,'sourceStableDuringTests':True,'checks':tests['checks'],'tests':tests,'benchmark':benchmark,'sourceHashes':before,'sanitizers':['address','undefined'],'method':__doc__,'limitations':['Comparison uses the real ArduinoJson 7.4.3 parser and current production language-reader header on the host.','Host timings do not predict SD transport or ESP32-P4 startup time.','Language selection/Preferences, file IO, mutex lifetime and application integration are tested separately.'],'evidenceHashes':{str(p.relative_to(ROOT)):sha(p) for p in sorted(proof.iterdir()) if p.is_file()}}
report['arduinoJsonVersion']=json.loads((a.arduino_json.parent/'library.json').read_text())['version']
(ROOT/'tools/verification/locale-parser-0851.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
