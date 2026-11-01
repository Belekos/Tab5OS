from pathlib import Path
import datetime,hashlib,json,os,subprocess,sys,re
here=Path(__file__).resolve().parent;release=here.parents[1];sketch=release/'Tab5OS_Arduino_v0.8.26_Ecriture'
build=Path('C:/workspace/tab5-sdr-ui-0826-host');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
files=['Tab5SdrUi.cpp','Tab5SdrUi.h','Tab5SdrModel.h','Tab5Sdr.h','Tab5Tools.h','Tab5TextInput.cpp','Tab5Focus.cpp','Tab5UiStyle.cpp','WriterDocument.cpp','Tab5ContextMenu.cpp']
before={n:digest(sketch/n) for n in files}
source=(sketch/'Tab5SdrUi.cpp').read_text(encoding='utf-8-sig');source=re.sub(r'^#include ["<].*?[">]\s*$','',source,flags=re.M)
(build/'ui.cpp').write_text((here/'ui.cpp').read_text(encoding='utf-8-sig').replace('/* PRODUCTION */',source),encoding='utf-8')
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-ui-0819-host/lvgl.lib')
flags=['/nologo','/utf-8','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_LABEL_TEXT_SELECTION=1','/DLV_USE_OS=0','/DLV_FONT_MONTSERRAT_24=1','/D_CRT_SECURE_NO_WARNINGS','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch)]
r=subprocess.run(['cl',*flags,'/EHsc','/std:c++17',str(build/'ui.cpp'),*[str(sketch/n) for n in ['Tab5TextInput.cpp','Tab5Focus.cpp','Tab5UiStyle.cpp','WriterDocument.cpp','Tab5ContextMenu.cpp']],str(lvgl/'src/font/lv_font_montserrat_24.c'),str(library),'/Fe:'+str(build/'ui.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace')
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(build/'ui.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=90)
report=dict(version='0.8.26',passed=r.returncode==0,physicalDeviceTested=False,sourceStableDuringTests=before=={n:digest(sketch/n) for n in files},sources=before,tests={n:digest(here/n) for n in ['ui.cpp','run_ui.py']},checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),output=r.stdout+r.stderr,checks=int(re.search(r'PASS (\d+)',r.stdout)[1]) if r.returncode==0 else 0,lifecycleCycles=100,bounds={'detections':2048,'scanSteps':20000,'iqBytes':4096,'fftPoints':1024},method='Complete production SDR UI source unchanged except includes; real LVGL, production Focus/TextInput/Style; USB worker and file I/O doubles.',limitations=['UI PSRAM allocations are tracked; LVGL general heap OOM is not injected.','File failure boundary verifies UI error handling; atomic write implementation separately tested by project storage tests.','No physical keyboard, SDR or display used.'])
(release/'tools/verification/sdr-ui-0826.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(r.stdout+r.stderr);raise SystemExit(0 if report['passed'] and report['sourceStableDuringTests'] else 1)
