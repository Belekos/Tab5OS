#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,json,os,subprocess,zipfile
p=argparse.ArgumentParser();p.add_argument('--sdk',type=Path,required=True);p.add_argument('--baseline-zip',type=Path,required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parents[1];sketch=next(r.glob('Tab5OS_Arduino_*_Ecriture'));build=r.parents[1]/'build_0845/audio-driver';build.mkdir(parents=True,exist_ok=True)
sdk=a.sdk.read_text()
def block(s,n):
 start=s.index(n);brace=s.index('{',start);depth=0
 for end in range(brace,len(s)):
  if s[end]=='{':depth+=1
  elif s[end]=='}':
   depth-=1
   if not depth:return s[start:end+1]
 raise ValueError(n)
actual_raw=block(sdk,'bool Speaker_Class::_play_raw(').replace('Speaker_Class::','FakeSpeaker::')
def stripped(s):return '\n'.join(x for x in s.splitlines() if not x.startswith('#include') and x!='#pragma once')
with zipfile.ZipFile(a.baseline_zip) as z:
 n=next(n for n in z.namelist() if n.endswith('/Tab5AudioDriver.cpp'));baseline=z.read(n).decode();baseline_h=z.read(n[:-4]+'.h').decode()
results={}
for name,source,header in [('before',baseline,baseline_h),('after',(sketch/'Tab5AudioDriver.cpp').read_text(),(sketch/'Tab5AudioDriver.h').read_text())]:
 source=stripped(source)
 f=(here/'driver_fixture.cpp').read_text().replace('// SDK_PLAY_RAW',actual_raw).replace('// ACTUAL_HEADER',stripped(header)).replace('// ACTUAL_DRIVER',source)
 f=f.replace('EXPECTED_FIRST_RATE','44100' if name=='before' else '48000').replace('EXPECTED_RESTARTS','1' if name=='before' else '0').replace('EXPECTED_OUTPUT_RATE','rate' if name=='before' else '48000')
 cpp=build/(name+'.cpp');cpp.write_text(f);exe=cpp.with_suffix('')
 proc=subprocess.run(['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie',str(cpp),'-o',str(exe)],capture_output=True,text=True);(build/(name+'-build.log')).write_text(proc.stdout+proc.stderr);print(proc.stderr[-7000:]);proc.check_returncode()
 proc=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));(build/(name+'-run.log')).write_text(proc.stdout+proc.stderr);print(proc.stdout+proc.stderr);proc.check_returncode();results[name]=json.loads(proc.stdout)
report={'version':'0.8.45','passed':True,'physicalDeviceTested':False,**results,'sourceHashes':{str(x.relative_to(r)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [sketch/'Tab5AudioDriver.cpp',sketch/'Tab5AudioDriver.h']},'testSourceHashes':{str(x.relative_to(r)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [here/'driver.py',here/'driver_fixture.cpp']},'sdkSpeakerSha256':hashlib.sha256(a.sdk.read_bytes()).hexdigest(),'scope':'Complete production AudioDriver.cpp/header, exact SDK Speaker::_play_raw metadata/admission. Hardware I2S/codec and mixer consumer are controlled boundaries; no acoustic output or physical resampling quality asserted. ASan/UBSan; LSan unavailable.'}
(r/'tools/verification/audio-driver-0845.json').write_text(json.dumps(report,indent=2)+'\n')
