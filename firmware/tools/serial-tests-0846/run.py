#!/usr/bin/env python3
import argparse,datetime,hashlib,json,pathlib,subprocess,zipfile,os
ap=argparse.ArgumentParser();ap.add_argument('--baseline',type=pathlib.Path,required=True);ap.add_argument('--build',type=pathlib.Path,required=True);args=ap.parse_args()
r=pathlib.Path(__file__).resolve().parents[2];s=next(r.glob('Tab5OS_Arduino_v*_Ecriture'));t=pathlib.Path(__file__).parent;b=args.build.resolve();b.mkdir(parents=True,exist_ok=True)
assert hashlib.sha256(args.baseline.read_bytes()).hexdigest()=='46fd5da89ebb693a61d3ab34e2d3f3480a714aff76c095ce339f290ee8641573'
def function(src,signature):
 start=src.index(signature);op=src.index('{',start);depth=1;i=op+1
 while depth:
  depth+=(src[i]=='{')-(src[i]=='}');i+=1
 return src[start:i]
results=[]
with zipfile.ZipFile(args.baseline) as z:
 for name in ('baseline','current'):
  out=b/name;out.mkdir(exist_ok=True)
  if name=='baseline':
   for n in ['Tab5Serial.cpp','Tab5Serial.h','Tab5SerialCore.h']:
    member=next(p for p in z.namelist() if p.endswith('/Tab5OS_Arduino_v0.8.45_Ecriture/'+n));(out/n).write_bytes(z.read(member))
   source=(out/'Tab5Serial.cpp').read_text()
  else:source=(s/'Tab5Serial.cpp').read_text()
  (out/'serial-production.inc').write_text(source[source.index('namespace Tab5Serial { namespace {'):])
  ui=(s/'Tab5SerialSettings.inc').read_text()
  (out/'ui-production.inc').write_text('static bool serialSettingsPending=false;static Tab5Serial::Config serialSettingsRequested;static std::vector<std::string> notices;static unsigned refreshes=0;static void toast(const char* x,unsigned){notices.emplace_back(x);}static void serialSettingsRefresh(){++refreshes;}\n'+function(ui,'static void serialSettingsService()'))
  cmd=['g++','-std=c++17','-O1','-g','-fno-omit-frame-pointer','-no-pie','-fsanitize=address,undefined','-I'+str(s if name=='current' else out),'-I'+str(out),str(t/'backend.cpp'),'-o',str(out/'test')]
  if name=='current':cmd.insert(1,'-DTEST_CURRENT')
  c=subprocess.run(cmd,text=True,capture_output=True);(out/'compile.log').write_text(c.stdout+c.stderr)
  assert c.returncode==0,c.stdout+c.stderr
  p=subprocess.run([str(out/'test')],text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));(out/'run.log').write_text(p.stdout+p.stderr);print(p.stdout+p.stderr);assert p.returncode==0
  results.append({'name':name,'compilerExitCode':c.returncode,'exitCode':p.returncode,'stdout':p.stdout,'stderr':p.stderr})
files=[s/'Tab5Serial.cpp',s/'Tab5Serial.h',s/'Tab5SerialCore.h',s/'Tab5SerialSettings.inc',t/'backend.cpp',pathlib.Path(__file__)]
report={'version':'0.8.46','checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'passed':True,'physicalDeviceTested':False,'leakSanitizer': 'Unavailable in environment (/proc thread inspection); disabled. No claim of LSan pass.','method':'Exact production Serial namespace and UI completion method, ASan/UBSan; UART/USB/NVS/FreeRTOS boundaries controlled. Baseline exact delivered ZIP. No physical timings claimed.','results':results,'sourceHashes':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}}
(r/'tools/verification/serial-async-0846.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
