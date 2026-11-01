#!/usr/bin/env python3
"""Compare exact released .41 production sources with .42 using the same deterministic fixtures."""
from pathlib import Path
import argparse, datetime, hashlib,json,os,re,subprocess,zipfile
p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path,required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--lvgl',type=Path,required=True);p.add_argument('--lvgl-build',type=Path,required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;root=here.parents[1];sketch=root/'Tab5OS_Arduino_v0.8.42_Ecriture';out=root/'tools/verification/display-0842';out.mkdir(exist_ok=True);a.build.mkdir(parents=True,exist_ok=True);base=a.build/'baseline';base.mkdir(exist_ok=True)
names=['Tab5DisplayPresent.cpp','Tab5DisplayPresent.h','Tab5DisplayPresentFence.h','Tab5DisplayCache.cpp','Tab5DisplayCache.h','Tab5DisplayCacheRanges.h','Tab5DisplayCacheRecovery.h','Tab5DisplayBandwidth.h','Tab5Mouse.cpp','Tab5Mouse.h']
with zipfile.ZipFile(a.baseline) as archive:
 for name in names:
  matches=[n for n in archive.namelist() if n.endswith('/Tab5OS_Arduino_v0.8.41_Ecriture/'+name)];assert len(matches)==1,(name,matches)
  (base/name).write_bytes(archive.read(matches[0]))
flags=['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build/'lv_conf.h')+'"','-I'+str(here/'stubs'),'-I'+str(a.lvgl)]
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
results={};commands={}
for variant in ('baseline','fixed'):
 source=base if variant=='baseline' else sketch
 extra=['-DTAB5_DISPLAY_BASELINE'] if variant=='baseline' else []
 for test in ('timing','mouse-burst'):
  executable=a.build/(variant+'-'+test)
  cmd=flags+extra+['-I'+str(source),'-I'+str(sketch),str(here/(test+'.cpp'))]
  if test=='timing':
   cmd += [str(source/'Tab5DisplayPresent.cpp'),str(source/'Tab5DisplayCache.cpp'),str(sketch/'Tab5DisplayActivity.cpp')]
   if variant=='fixed':cmd += [str(sketch/'Tab5DisplayRefresh.cpp')]
  cmd += [str(a.lvgl_build/'liblvgl.a'),'-pthread','-o',str(executable)]
  key=variant+'-'+test;commands[key]=cmd
  build=subprocess.run(cmd,capture_output=True,text=True);(out/(key+'-build.log')).write_text(build.stdout+build.stderr)
  if build.returncode:print(build.stderr);build.check_returncode()
  run=subprocess.run([str(executable)],capture_output=True,text=True,env=env);(out/(key+'-run.log')).write_text(run.stdout+run.stderr);print(key,run.stdout+run.stderr,end='');run.check_returncode()
  results[key]={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',run.stdout)}
report=dict(version='0.8.42',passed=True,physicalDeviceTested=False,checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),baselineArchiveSha256=hashlib.sha256(a.baseline.read_bytes()).hexdigest(),results=results,commands=commands,scope=__doc__,model=dict(dmaIntervalMs=16,loopDelayMs=8,refreshPeriodMs=16,realLVGL='9.6.0',sanitizers=['address','undefined']),limitations=['The timings count simulated waits and deterministic scheduling only, not CPU time, measured physical latency or frames per second.','Real LVGL and production mouse/display sources run; USB and cache/DMA/SDK boundaries use explicit doubles.','Mouse comparison covers only already-queued reports. Existing producer queue overflow is not repaired by this change.','Conservative two-callback framebuffer reuse fence remains unchanged.'])
report['sourceHashes']={str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [*sketch.glob('Tab5Display*'),sketch/'Tab5Mouse.cpp',sketch/'Tab5Mouse.h']}
report['testSourceHashes']={str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [here/'compare.py',here/'timing.cpp',here/'mouse-burst.cpp',here/'pipeline.cpp']}
(out.parent/'display-latency-0842.json').write_text(json.dumps(report,indent=2)+'\n')
