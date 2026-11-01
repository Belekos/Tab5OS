from pathlib import Path
import datetime,hashlib,json,os,subprocess,sys,re
here=Path(__file__).resolve().parent;release=here.parents[1];sketch=release/'Tab5OS_Arduino_v0.8.25_Ecriture'
build=Path('C:/workspace/tab5-sdr-usb-0825-host');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def block(text,key):
    start=text.index(key);brace=text.index('{',start);depth=1;end=brace+1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
files=['src/rtl_sdr/usb_adapter.cpp','src/rtl_sdr/libusb.h','src/rtl_sdr/librtlsdr.c','src/rtl_sdr/rtl-sdr.h']
before={n:digest(sketch/n) for n in files}
source=(sketch/files[0]).read_text(encoding='utf-8-sig');source=re.sub(r'^#include ["<].*?[">]\s*$','',source,flags=re.M)
test=(here/'usb.cpp').read_text(encoding='utf-8-sig').replace('/* PRODUCTION */',source);driver=(sketch/'src/rtl_sdr/librtlsdr.c').read_text(encoding='utf-8');test=test.replace('/* INPUT_SETTER */',block(driver,'int tab5_rtlsdr_set_input_and_freq(')).replace('/* RESET_BUFFER */',block(driver,'int rtlsdr_reset_buffer('))
(build/'usb.cpp').write_text(test,encoding='utf-8')
r=subprocess.run(['cl','/nologo','/utf-8','/EHsc','/std:c++17','/O2','/D_CRT_SECURE_NO_WARNINGS','/I'+str(sketch),str(build/'usb.cpp'),'/Fe:'+str(build/'usb.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace')
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(build/'usb.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60)
report=dict(version='0.8.25',passed=r.returncode==0,physicalDeviceTested=False,sourceStableDuringTests=before=={n:digest(sketch/n) for n in files},sources=before,tests={n:digest(here/n) for n in ['usb.cpp','run_usb.py']},checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),output=r.stdout+r.stderr,checks=int(re.search(r'PASS (\d+)',r.stdout)[1]) if r.returncode==0 else 0,lifecycleCycles=2000,method='Complete production USB adapter except includes; two production librtlsdr setters extracted unchanged; ESP-IDF boundary simulation with DMA allocation/ownership tracking.',limitations=['No physical RTL-SDR or USB host transport used.','Endpoint zero cancellation preserves DMA until the SDK completion callback; a permanently unresponsive device still requires unplugging.'])
(release/'tools/verification/sdr-usb-0825.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(r.stdout+r.stderr);raise SystemExit(0 if report['passed'] and report['sourceStableDuringTests'] else 1)
