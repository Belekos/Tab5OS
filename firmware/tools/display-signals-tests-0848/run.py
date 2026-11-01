#!/usr/bin/env python3
import pathlib,subprocess,json,hashlib,argparse,os
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path,required=True);a=p.parse_args();a.build.mkdir(parents=True,exist_ok=True)
root=pathlib.Path(__file__).resolve().parents[2];src=root/'Tab5OS_Arduino_v0.8.48_Ecriture';test=pathlib.Path(__file__).resolve().parent
files=[src/'Tab5DisplaySignals.cpp',src/'Tab5DisplaySignals.h'] + sorted(p for p in test.rglob('*') if p.is_file())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before={str(p.relative_to(root)):sha(p) for p in files};results={}
for mode in ('wrapped','unwrapped'):
 exe=a.build.resolve()/mode
 cmd=['g++','-std=c++17','-Wall','-Wextra','-Werror','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(test/'stubs'),'-I'+str(src),str(src/'Tab5DisplaySignals.cpp'),str(test/'signals.cpp'),str(test/'signals-sdk.cpp'),'-o',str(exe)]
 if mode=='wrapped':cmd+=['-Wl,--wrap=esp_intr_alloc']
 r=subprocess.run(cmd,capture_output=True,text=True);(a.build/(mode+'-build.log')).write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
 r=subprocess.run([str(exe),mode],capture_output=True,text=True,env={**os.environ,"ASAN_OPTIONS":"detect_leaks=0"});(a.build/(mode+'-run.log')).write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
 results[mode]=r.stdout.strip()
assert before=={str(p.relative_to(root)):sha(p) for p in files}
report={'version':'0.8.48','passed':True,'sourceHashes':before,'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],'physicalDeviceTested':False,'leakSanitizerEnabled':False,'leakSanitizerReason':'Runtime ptrace prevents LSan; production Signals performs no allocation.','results':results,'limitations':['Host controlled IRQ/clock; does not reproduce real panel bandwidth or interrupt timing.']}
out=root/'tools/verification/display-signals-0848.json';out.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
