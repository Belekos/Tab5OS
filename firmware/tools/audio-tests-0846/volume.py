#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,json,os,subprocess,zipfile
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',type=Path,required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parents[1];s=next(r.glob('Tab5OS_Arduino_*_Ecriture'));b=r.parents[1]/'build_0846/audio-volume';b.mkdir(parents=True,exist_ok=True)
def block(s,n):
 start=s.index(n);brace=s.index('{',start);depth=0
 for end in range(brace,len(s)):
  if s[end]=='{':depth+=1
  elif s[end]=='}':
   depth-=1
   if not depth:return s[start:end+1]
 raise ValueError(n)
with zipfile.ZipFile(a.baseline_zip) as z:baseline=z.read(next(n for n in z.namelist() if n.endswith('/Tab5Audio.cpp'))).decode()
results={}
for name,text in [('before',baseline),('after',(s/'Tab5Audio.cpp').read_text())]:
 methods=block(text,'class Tab5AudioEngine::StateGuard')+';\n'+'\n'.join(block(text,n) for n in ['bool Tab5AudioEngine::startLocalDecoder(', 'bool Tab5AudioEngine::startRadioNow(', 'void Tab5AudioEngine::setVolume('])
 cpp=b/(name+'.cpp');cpp.write_text((here/'volume_fixture.cpp').read_text().replace('// PRODUCTION_METHODS',methods).replace('EXPECTED_UNPROTECTED_READS','301' if name=='before' else '0'));exe=cpp.with_suffix('')
 proc=subprocess.run(['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined','-fno-pie','-no-pie',str(cpp),'-o',str(exe)],capture_output=True,text=True);(b/(name+'-build.log')).write_text(proc.stdout+proc.stderr);print(proc.stderr);proc.check_returncode()
 proc=subprocess.run([str(exe)],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));(b/(name+'-run.log')).write_text(proc.stdout+proc.stderr);print(proc.stdout+proc.stderr);proc.check_returncode();results[name]=json.loads(proc.stdout)
report={'version':'0.8.46','passed':True,'physicalDeviceTested':False,'baselineArchiveSha256':hashlib.sha256(a.baseline_zip.read_bytes()).hexdigest(),**results,'sourceHashes':{str((s/'Tab5Audio.cpp').relative_to(r)):hashlib.sha256((s/'Tab5Audio.cpp').read_bytes()).hexdigest()},'testSourceHashes':{str(x.relative_to(r)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [here/'volume.py',here/'volume_fixture.cpp']},'scope':'Complete production startLocalDecoder/startRadioNow/setVolume and actual StateGuard. Guarded field boundary records unprotected volume conversions; slow SDK/files/network/decoder boundaries assert no state mutex held. 300 successful starts plus refused I2S claim. Controlled dependencies, no real FreeRTOS race or acoustic/device validation.'}
(r/'tools/verification/audio-volume-0846.json').write_text(json.dumps(report,indent=2)+'\n')
