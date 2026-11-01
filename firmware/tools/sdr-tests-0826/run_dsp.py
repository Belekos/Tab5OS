from pathlib import Path
import datetime, hashlib, json, os, subprocess, sys, re
here=Path(__file__).resolve().parent;release=here.parents[1];sketch=release/'Tab5OS_Arduino_v0.8.26_Ecriture'
build=Path('C:/workspace/tab5-sdr-dsp-0826-host');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
baseline=here/'fixtures/dsp-0824.h'
assert digest(baseline)=='af532a96ae11d2cf99d6d63e2264adea121e35653dbe540dcf8c6d991a40402a', 'Immutable 0.8.24 DSP fixture changed'
(build/'baseline-dsp.h').write_text(baseline.read_text(encoding='utf-8-sig').replace('namespace Tab5SdrDsp','namespace BaselineSdrDsp'),encoding='utf-8')
before=digest(sketch/'Tab5SdrDsp.h')
command=['cl','/nologo','/utf-8','/EHsc','/std:c++17','/W4','/O2','/D_CRT_SECURE_NO_WARNINGS','/I'+str(sketch),'/I'+str(build),str(here/'dsp.cpp'),'/Fe:'+str(build/'dsp.exe')]
r=subprocess.run(command,cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace')
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(build/'dsp.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=90)
report=dict(version='0.8.26',passed=r.returncode==0,physicalDeviceTested=False,sourceStableDuringTests=before==digest(sketch/'Tab5SdrDsp.h'),sources={'Tab5SdrDsp.h':before},baseline={'version':'0.8.24','sha256':digest(baseline)},tests={n:digest(here/n) for n in ['dsp.cpp','run_dsp.py']},checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),output=r.stdout+r.stderr,checks=int(re.search(r'PASS (\d+)',r.stdout)[1]) if r.returncode==0 else 0,method='Production DSP processing continuous synthetic quantized 8-bit IQ, broadcast mono and stereo multiplex including pilot and RDS. Original 0.8.24 DSP replayed against identical IQ. Spectral measurements after settling; variable amplitude, carrier offset, exact odd/short USB fragmentation and retune reset.',limitations=['No physical antenna, receiver, oscillator or audio output measured.','Synthetic noise is bounded and does not model multipath or adjacent-channel overload.'])
(release/'tools/verification/sdr-dsp-0826.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(r.stdout+r.stderr);raise SystemExit(0 if report['passed'] and report['sourceStableDuringTests'] else 1)
