"""Execute production controller navigation and Python snapshot bridge."""
from pathlib import Path
import argparse, hashlib, json, subprocess, tempfile, importlib.util, sys, types, struct
p=argparse.ArgumentParser();p.add_argument('sketch',type=Path);a=p.parse_args()
sketch=a.sketch.resolve();here=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='tab5-pad-navigation-') as tmp:
 d=Path(tmp);cmd=d/'run.cmd'
 cmd.write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl /nologo /utf-8 /EHsc /std:c++17 /I"'+str(sketch)+'" "'+str(here/'test_navigation.cpp')+'" "'+str(sketch/'WriterGamepadNavigation.cpp')+'" /Fe:navigation.exe\nif errorlevel 1 exit /b 1\nnavigation.exe\n',encoding='utf-8')
 r=subprocess.run(['cmd','/d','/c',str(cmd)],cwd=d,capture_output=True,text=True,encoding='utf-8',errors='replace');print(r.stdout+r.stderr)
 if r.returncode:raise SystemExit(r.returncode)
 data=struct.pack('<7I2H4h8B16B',1,16,1,2,0,0,0,0x2dc8,0x6001,-32768,32767,0,0,4,3,0,1,9,0,0,0,*range(16))
 commands=[]
 def call(*args):
  commands.append(args)
  assert args[0]==106
  return data if args[1]==0 else 0
 sys.modules['_tab5']=types.SimpleNamespace(call=call)
 path=sketch/'sdcard/MicroPython/lib/gamepad.py';spec=importlib.util.spec_from_file_location('gamepad',path);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
 state=mod.state();assert state['connected'] and state['axes']==(-32768,32767,0,0) and state['buttons']==mod.A and state['raw_report']==bytes(range(9))
 assert mod.buttons()==mod.A;mod.capture(True);assert commands[-1]==(106,1,1);mod.capture(False);assert commands[-1]==(106,1,0)
 print('PASS: actual gamepad.py bridge decoding, constants and capture calls.')
 files=[sketch/'WriterGamepadNavigation.cpp',sketch/'WriterGamepadNavigation.h',path,here/'test_navigation.cpp',Path(__file__)]
 report=dict(version='0.8.24',passed=True,exitCode=r.returncode,pythonCases=['state presence, axes, A mask, raw_report', 'buttons mask', 'capture(True) bridge arguments', 'capture(False) bridge arguments'],pythonOutput='PASS: actual gamepad.py bridge decoding, constants and capture calls.',physicalDeviceTested=False,method='Production C++ navigation/serialization with USB snapshot boundary mocked; actual Python module with _tab5 bridge boundary mocked.',output=r.stdout+r.stderr,sources={str(f.relative_to(sketch.parent)).replace('\\','/'):hashlib.sha256(f.read_bytes()).hexdigest() for f in files})
 (sketch.parent/'tools/verification/gamepad-navigation-0824.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
