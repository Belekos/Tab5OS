from pathlib import Path
import argparse,subprocess,hashlib,json
p=argparse.ArgumentParser();p.add_argument('sketch',type=Path);a=p.parse_args()
sketch=a.sketch.resolve();here=Path(__file__).resolve().parent
build=Path('C:/workspace/tab5-mouse-context-0824');build.mkdir(exist_ok=True)
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');lib=Path('C:/workspace/tab5-ui-0822-host/lvgl.lib')
assert lib.is_file(),'Run the existing UI LVGL host build first'
flags=['/nologo','/utf-8','/EHsc','/std:c++17','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_LABEL_TEXT_SELECTION=1','/DLV_USE_OS=0','/D_CRT_SECURE_NO_WARNINGS','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch)]
cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" >nul\ncl '+' '.join('"'+arg+'"' for arg in flags+[str(here/'test_mouse.cpp'),str(lib),'/Fe:mouse.exe'])+'\nif errorlevel 1 exit /b 1\nmouse.exe\n',encoding='utf-8')
r=subprocess.run(['cmd','/d','/c',str(cmd)],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace');print((r.stdout+r.stderr)[-6000:])
files=[sketch/n for n in ['Tab5Mouse.cpp','Tab5Mouse.h','WriterUsb.h','WriterKeys.h','WriterMouse.h','WriterGamepad.h']]+[here/'test_mouse.cpp',Path(__file__)]
report=dict(version='0.8.24',passed=r.returncode==0,exitCode=r.returncode,physicalDeviceTested=False,method='Actual LVGL 9.6.0 with full production Tab5Mouse.cpp. USB connection/report queue boundaries mocked; no physical mouse.',output=r.stdout+r.stderr,sources={str(f.relative_to(sketch.parent)).replace('\\','/'):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},lvglArchiveSha256=hashlib.sha256(lib.read_bytes()).hexdigest())
(sketch.parent/'tools/verification/mouse-context-0824.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(r.returncode)
