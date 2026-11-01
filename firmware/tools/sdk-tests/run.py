from pathlib import Path
import argparse,datetime,hashlib,json,os,runpy,shutil,struct,subprocess,sys,tempfile,types
p=argparse.ArgumentParser();p.add_argument('--clang-bin',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];sketch=next(root.glob('Tab5OS_Arduino_v*_Ecriture'));here=Path(__file__).resolve().parent
build=sketch/'tools/native-apps/build.py';checks=[]
def run(args,cwd=None,expected=0):
 r=subprocess.run(args,cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace')
 checks.append({'command':[str(v) for v in args],'passed':r.returncode==expected,'exitCode':r.returncode,'output':(r.stdout+r.stderr)[-3500:]})
 if r.returncode!=expected:print(checks[-1]['output']);raise AssertionError(args)
 return r
with tempfile.TemporaryDirectory(prefix='tab5-sdk-0822-',dir=root.parent) as temporary:
 d=Path(temporary);elfs=[]
 for example in ['hash_file','pattern']:
  out=d/(example+'.app.elf')
  run([sys.executable,str(build),str(sketch/'tools/native-apps/examples'/f'{example}.c'),'--compiler','clang','--toolchain',str(a.clang_bin),'-o',str(out)])
  data=out.read_bytes();assert data[:6]==b'\x7fELF\x01\x01';assert struct.unpack_from('<HH',data,16)==(3,243)
  assert struct.unpack_from('<I',data,36)[0]&6==0 # ilp32 soft-float ABI
  assert struct.unpack_from('<I',data,24)[0]!=0
  symbols=run([str(a.clang_bin/'llvm-readelf.exe'),'-Ws',str(out)]).stdout
  imports={line.split()[-1] for line in symbols.splitlines() if ' UND ' in line and len(line.split())>=8}
  expected={'tab5_app_api_v1' if example=='hash_file' else 'tab5_app_api_v2'}
  assert imports==expected and 'tab5_app_main' in symbols
  elfs.append({'example':example,'elfClass':'ELF32','machine':'RISC-V','type':'DYN','abi':'ilp32','imports':sorted(imports),'entryPoint':'tab5_app_main','sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)})
 # Forbidden import rejection must preserve a previously good destination.
 invalid=d/'invalid.c';invalid.write_text('extern int forbidden_import(void); int tab5_app_main(int n,char **v){return forbidden_import();}\n')
 out=d/'preserve.app.elf';out.write_bytes(b'previous-good-image')
 run([sys.executable,str(build),str(invalid),'--compiler','clang','--toolchain',str(a.clang_bin),'-o',str(out)],expected=1)
 assert out.read_bytes()==b'previous-good-image';assert not list(d.glob('.tab5-build-*'))
 vcvars='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
 cmd=d/'native.cmd';cmd.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /TC /std:c17 /utf-8 /I"{sketch}" "{here / "test_pattern.c"}" "{sketch / "tools/native-apps/examples/pattern.c"}" /Fe:pattern.exe\nif errorlevel 1 exit /b 1\npattern.exe\n')
 run(['cmd','/c',str(cmd)],cwd=d)
 # Execute actual Python template, with events and cleanup at UI boundaries.
 template=root/'examples/micropython/compteur/main.py';widgets=[];events=[];closed=[];waits=[]
 ui=types.ModuleType('tab5ui');ui.WIDTH=1100;ui.HEIGHT=500;ui.open=lambda title:None;ui.tr=lambda value:value;ui.close=lambda:closed.append(True);ui.shortcut=lambda widget,key:widget
 class Widget:
  def __init__(self,text,**kwargs):self.id=len(widgets)+1;self.current=text;widgets.append(self)
  def text(self,value):self.current=value
 ui.label=ui.button=Widget;ui.is_open=lambda:True;ui.poll=lambda:events.pop(0)
 fake_time=types.ModuleType('time');fake_time.sleep_ms=lambda value:waits.append(value)
 old_ui=sys.modules.get('tab5ui');old_time=sys.modules['time'];sys.modules['tab5ui']=ui;sys.modules['time']=fake_time
 try:
  events[:]=[(3,0),(3,0),(2,0),(4,0),(3,0),(5,0)]
  runpy.run_path(str(template));assert widgets[0].current==1 and len(closed)==1 and len(waits)==5
  widgets.clear();events.clear()
  try:runpy.run_path(str(template))
  except IndexError:pass
  else:raise AssertionError('injected event failure did not propagate')
  assert len(closed)==2
 finally:
  sys.modules['time']=old_time
  if old_ui is None:sys.modules.pop('tab5ui',None)
  else:sys.modules['tab5ui']=old_ui
 checks.append({'name':'actual Python template','passed':True,'events':6,'cleanupOnFailure':True})
 # Portable sketch discovery: check-only performs no install/compile/flash.
 compiler=d/'Compiler_Tab5OS.ps1';shutil.copy2(root/compiler.name,compiler)
 one=d/'Tab5OS_Arduino_v0.8.22_Ecriture';one.mkdir();(one/(one.name+'.ino')).touch()
 ps=shutil.which('powershell') or shutil.which('pwsh');assert ps
 base=[ps,'-NoProfile','-ExecutionPolicy','Bypass','-File',str(compiler)]
 assert '0.8.22' in run(base+['-CheckOnly']).stdout
 two=d/'Tab5OS_Arduino_v0.8.23_Ecriture';two.mkdir();(two/(two.name+'.ino')).touch()
 run(base+['-CheckOnly'],expected=1)
 assert '0.8.23' in run(base+['-CheckOnly','-SketchPath',str(two)]).stdout
 run(base+['-CheckOnly','-SketchPath',str(d/'missing')],expected=1)
 paths=[build,sketch/'Tab5AppApi.h',sketch/'tools/native-apps/examples/pattern.c',sketch/'tools/native-apps/examples/hash_file.c',template,root/'Compiler_Tab5OS.ps1',Path(__file__),here/'test_pattern.c']
 report=dict(version='0.8.25',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=all(c['passed'] for c in checks),physicalDeviceTested=False,elfExamples=elfs,checks=checks,sourceHashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
 (root/'tools/verification/sdk-0825.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
 print('PASS: two Clang/LLD RISC-V SDK examples; forbidden imports preserve existing image; C/Python lifecycle; PowerShell discovery')
