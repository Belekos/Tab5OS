#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib, json, os, re, subprocess, zipfile
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',required=True,type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parents[1];sketch=next(r.glob('Tab5OS_Arduino_*_Ecriture'))
b=r.parents[1]/'build_0847/audio-sdr';b.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(a.baseline_zip) as z: before=z.read(next(n for n in z.namelist() if n.endswith('/Tab5Sdr.cpp'))).decode()
results={}
for label,source in [('before',before),('after',(sketch/'Tab5Sdr.cpp').read_text())]:
 source=re.sub(r'^#include ["<](?!atomic|new).*?[">]\s*$','',source,flags=re.M)
 text=(here/'sdr_fixture.cpp').read_text().replace('/* PRODUCTION */',source)
 cpp=b/(label+'.cpp');cpp.write_text(text);binary=b/label
 command=['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-pie','-no-pie','-DPATCHED='+('1' if label=='after' else '0'),'-I'+str(sketch),str(cpp),'-o',str(binary)]
 process=subprocess.run(command,capture_output=True,text=True);(b/(label+'-build.log')).write_text(process.stdout+process.stderr)
 if process.returncode: print(process.stderr)
 process.check_returncode()
 process=subprocess.run([str(binary)],capture_output=True,text=True,timeout=120,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
 (b/(label+'-run.log')).write_text(process.stdout+process.stderr);print(process.stdout+process.stderr);process.check_returncode()
 results[label]={'passed':True,'checks':int(re.search(r'PASS (\d+)',process.stdout)[1]),'output':process.stdout}
report={'version':'0.8.47','passed':True,**results,'sourceHashes':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [sketch/'Tab5Sdr.cpp',sketch/'Tab5Sdr.h',sketch/'Tab5SdrDsp.h',sketch/'Tab5SdrModel.h']},'testHashes':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__),here/'sdr_fixture.cpp']},'scope':'Complete production SDR worker and models/DSP, USB/audio/RTOS controlled boundaries. 2000 lifecycle cycles, PCM sink failure retains spectrum, explicit-command retry, deferred release before retirement. ASan/UBSan, no physical RTL-SDR test.'}
(r/'tools/verification/audio-sdr-0847.json').write_text(json.dumps(report,indent=2)+'\n')
