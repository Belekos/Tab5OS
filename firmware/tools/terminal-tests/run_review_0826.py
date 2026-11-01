from pathlib import Path
import hashlib,json,os,subprocess,sys,shutil,tempfile
release=Path(__file__).resolve().parents[2];sketch=release/'Tab5OS_Arduino_v0.8.26_Ecriture';here=Path(__file__).resolve().parent
build=Path('C:/workspace/tab5-shell-review-0826');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
 p=build/'run.cmd';p.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(p)]))
shutil.copy2(here.parent/'network-tests/ShellPlatform.h',build/'ShellPlatform.h')
for header in ['unistd.h','dirent.h','fnmatch.h']:(build/header).write_text('#include "ShellPlatform.h"\n',encoding='utf-8')
r=subprocess.run(['cl','/nologo','/std:c++17','/EHsc','/MDd','/D_DEBUG','/wd4996','/wd4267','/utf-8','/DARDUINO','/DTAB5_SHELL_HOST','/I'+str(build),'/I'+str(sketch),str(here/'review.cpp'),'/Fe:'+str(build/'review.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace');print(r.stdout+r.stderr);r.check_returncode()
with tempfile.TemporaryDirectory(prefix='volume-',dir=build) as d:r=subprocess.run([str(build/'review.exe'),d],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace')
print(r.stdout+r.stderr);report=json.loads(r.stdout.strip().splitlines()[-1]);report['version']='0.8.26';report['output']=r.stdout+r.stderr;report['sources']={str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [sketch/'Tab5Shell.cpp',sketch/'Tab5Shell.h',here/'review.cpp',Path(__file__)]};report['method']='Independent adversarial checks of real Engine with host filesystem; substitution output token boundaries, quoting, stderr, failed/cancelled transactional redirection';report['hardwareTested']=False
(release/'tools/verification/shell-review-0826.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');raise SystemExit(r.returncode)
