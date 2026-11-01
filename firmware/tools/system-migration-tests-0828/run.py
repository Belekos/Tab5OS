from pathlib import Path
import hashlib,json,os,subprocess,sys
here=Path(__file__).resolve().parent;release=here.parent.parent
sketch=release/'Tab5OS_Arduino_v0.8.24_Ecriture';build=Path('C:/workspace/tab5-system-migration-0824');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    cmd=build/'run.cmd'
    cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
compiled=subprocess.run(['cl','/nologo','/utf-8','/EHsc','/std:c++17','/fsanitize=address','/Zi','/I'+str(here),'/I'+str(sketch),str(here/'migration.cpp'),str(sketch/'Tab5SystemMigration.cpp'),'/Fe:'+str(build/'migration.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace')
(build/'compile.log').write_text(compiled.stdout+compiled.stderr,encoding='utf-8')
if compiled.returncode:print(compiled.stdout+compiled.stderr);raise SystemExit(compiled.returncode)
result=subprocess.run([str(build/'migration.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60)
print(result.stdout+result.stderr)
paths=[sketch/'Tab5SystemMigration.cpp',sketch/'Tab5SystemMigration.h',here/'migration.cpp',here/'Arduino.h',here/'FS.h',here/'run.py']
report={'version':'0.8.24','passed':result.returncode==0,'physicalDeviceTested':False,
 'method':'Complete production migration module compiled with MSVC AddressSanitizer. In-memory Arduino FS/String boundary retains exact file payloads, injected errors at every observed FS boundary and interruptions after each successful metadata mutation. Conflicts, resource policy, file/directory swaps, resume/idempotence, 10000-tile subtree, reserved-path safety, denied storage, deep and long paths. Not a physical FAT power-cut test.',
 'result':{'exitCode':result.returncode,'output':result.stdout+result.stderr},
 'sources':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}}
(release/'tools/verification/system-migration-0824.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(result.returncode)
