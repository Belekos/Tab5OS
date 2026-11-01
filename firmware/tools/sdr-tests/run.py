from pathlib import Path
import datetime,hashlib,json,os,subprocess,sys,re
here=Path(__file__).resolve().parent
release=here.parents[1]
sketch=release/'Tab5OS_Arduino_v0.8.25_Ecriture'
build=Path('C:/workspace/tab5-sdr-0825-host');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
files=['Tab5Sdr.cpp','Tab5Sdr.h','Tab5SdrModel.h','Tab5SdrDsp.h']
before={n:digest(sketch/n) for n in files}
source=(sketch/'Tab5Sdr.cpp').read_text(encoding='utf-8-sig')
source=re.sub(r'^#include ["<](?!atomic|new).*?[">]\s*$', '',source,flags=re.M)
(build/'worker.cpp').write_text((here/'worker.cpp').read_text(encoding='utf-8-sig').replace('/* PRODUCTION */',source),encoding='utf-8')
command=['cl','/nologo','/utf-8','/EHsc','/std:c++17','/W4','/O2','/D_CRT_SECURE_NO_WARNINGS','/I'+str(sketch),str(build/'worker.cpp'),'/Fe:'+str(build/'worker.exe')]
r=subprocess.run(command,cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace')
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(build/'worker.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8',timeout=90)
report=dict(version='0.8.25',passed=r.returncode==0,physicalDeviceTested=False,sourceStableDuringTests=before=={n:digest(sketch/n) for n in files},sources=before,tests={n:digest(here/n) for n in ['worker.cpp','run.py']},checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),output=r.stdout+r.stderr,checks=int(re.search(r'PASS (\d+)',r.stdout)[1]) if r.returncode==0 else 0,lifecycleCycles=2000,bounds={'detections':128,'scanSteps':20000,'iqBytes':4096,'fftPoints':1024},method='Production SDR worker source unchanged except hardware includes; production models and DSP. USB/RTOS/audio boundary doubles, PSRAM allocation tracking and injected failure paths.',limitations=['No physical RTL-SDR dongle or ADC measured.','USB host transport and tuner implementation are not replaced by these mocked boundary checks.','Real SRAM and DMA usage must be measured on P4.'])
(release/'tools/verification/sdr-worker-0825.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(r.stdout+r.stderr);raise SystemExit(0 if report['passed'] and report['sourceStableDuringTests'] else 1)
