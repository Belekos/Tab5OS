from pathlib import Path
import datetime,hashlib,json,os,subprocess,sys
here=Path(__file__).resolve().parent;release=here.parents[1];sketch=release/'Tab5OS_Arduino_v0.8.26_Ecriture';build=Path('C:/workspace/tab5-share-admission-0826');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
 cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
s=(sketch/'Tab5FileShare.cpp').read_text(encoding='utf-8');start=s.index('bool tab5FileShareStart(');opening=s.index('{',start);end=opening+1;depth=1
while depth:
 if s[end]=='{':depth+=1
 elif s[end]=='}':depth-=1
 end+=1
source=(here/'share_start.cpp').read_text(encoding='utf-8').replace('/* SHARE_START */',s[start:end]);(build/'share.cpp').write_text(source,encoding='utf-8')
r=subprocess.run(['cl','/nologo','/utf-8','/EHsc','/std:c++17','/D_CRT_SECURE_NO_WARNINGS',str(build/'share.cpp'),'/Fe:'+str(build/'share.exe')],cwd=build,capture_output=True,encoding='utf-8',errors='replace')
if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(build/'share.exe')],capture_output=True,encoding='utf-8',errors='replace');print(r.stdout+r.stderr)
report=dict(version='0.8.26',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=r.returncode==0,output=r.stdout+r.stderr,method='Actual production tab5FileShareStart with controlled SDK boundaries; 10000 failed-start scenarios: context/page OOM, page generation failure, task creation failure, OS memory admission denial. No radio/physical test.',physicalDeviceTested=False,sources={n:hashlib.sha256((sketch/n).read_bytes()).hexdigest() for n in ['Tab5FileShare.cpp','Tab5Task.h']},tests={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__),here/'share_start.cpp']})
(release/'tools/verification/share-admission-0826.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');raise SystemExit(r.returncode)
