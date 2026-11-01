"""Execute WriterUsb callback and public readers, not a copy of their logic."""
from pathlib import Path
import argparse,datetime,hashlib,json,re,subprocess
p=argparse.ArgumentParser();p.add_argument('sketch',type=Path);a=p.parse_args()
sketch=a.sketch.resolve();here=Path(__file__).resolve().parent
build=Path('C:/workspace/tab5-gamepad-pipeline-0824');build.mkdir(exist_ok=True)
source=(sketch/'WriterUsb.cpp').read_text(encoding='utf-8')
def function(name):
 m=re.search(r'^(?:void|bool|uint32_t) '+name+r'\([^;{]*\)\s*\{',source,re.M)
 assert m,name
 start=m.start();depth=1
 for token in re.finditer(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\{|\}',source[m.end():],re.S):
  if token.group()=='{':depth+=1
  elif token.group()=='}':
   depth-=1
   if depth==0:return source[start:m.end()+token.end()]
 raise RuntimeError(name)
names=['releasePad','interfaceEvent','usbGamepadButtons','usbGameButtons','usbGamepadInfo','serviceRecovery']
structs=[next(line for line in source.splitlines() if line.startswith('struct '+n+'{')) for n in ['PadState']]
interface=re.search(r'struct Interface \{.*?\};',source).group()
prefix='''constexpr unsigned MaxInterfaces=8;
std::atomic<bool> detachPending[MaxInterfaces],recoverPending[MaxInterfaces];uint32_t recoverAt[MaxInterfaces]{};std::atomic<uint32_t> gameButtons{0},mouseCount{0};
std::atomic<bool> padActivity{false},helpPressed{false};QueueHandle_t mouseReports=nullptr;
struct Report {HidKeys keys;bool disconnected;uint8_t slot=0;};
'''+interface+'\nInterface* interfaces=nullptr;\n'+''.join(structs)+'''
PadState pads[MaxInterfaces];
void publishKeyboard(Interface&,bool=false){}
'''
(build/'pipeline_production.inc').write_text(prefix+'\n'.join(function(n) for n in names),encoding='utf-8')
cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl /nologo /utf-8 /EHsc /std:c++17 /I"'+str(sketch)+'" /I"'+str(build)+'" "'+str(here/'test_pipeline.cpp')+'" "'+str(sketch/'WriterGamepad.cpp')+'" "'+str(sketch/'WriterHid.cpp')+'" "'+str(sketch/'WriterMouse.cpp')+'" /Fe:pipeline.exe\nif errorlevel 1 exit /b 1\npipeline.exe\n',encoding='utf-8')
r=subprocess.run(['cmd','/d','/c',str(cmd)],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace');print((r.stdout+r.stderr)[-8000:])
files=[sketch/n for n in ['WriterUsb.cpp','WriterGamepad.cpp','WriterGamepad.h','WriterHid.cpp','WriterHid.h','WriterMouse.cpp','WriterMouse.h']]+[Path(__file__),here/'test_pipeline.cpp']
report=dict(version='0.8.24',passed=r.returncode==0,exitCode=r.returncode,physicalDeviceTested=False,method='MSVC: extracted production WriterUsb interfaceEvent, releasePad, usbGameButtons, usbGamepadInfo; actual HID/XInput decoders. USB input/RTOS queue boundaries mocked; protocol fixtures, not physical captures.',extractedFunctions=names,output=r.stdout+r.stderr,sources={str(f.relative_to(sketch.parent)).replace('\\','/'):hashlib.sha256(f.read_bytes()).hexdigest() for f in files})
(sketch.parent/'tools/verification/gamepad-pipeline-0824.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(r.returncode)
