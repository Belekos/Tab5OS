"""Compile the map engine with the installed P4 SDK and retain actual frames."""
from pathlib import Path
import hashlib,json,subprocess
here=Path(__file__).resolve().parent;release=here.parent.parent
sketch=release/'Tab5OS_Arduino_v0.8.26_Ecriture'
build=Path('C:/workspace/tab5-maps-0826');build.mkdir(exist_ok=True)
database_path=Path('C:/workspace/tab5-build-0825-clang/compile_commands.json')
database=json.loads(database_path.read_text())
entry=next(x for x in database if Path(x['file']).name=='Tab5Map.cpp')
args=[];skip=False
for arg in entry['arguments']:
 if skip:skip=False;continue
 if arg=='-o':skip=True;continue
 if arg==entry['file']:continue
 args.append(arg.replace('Tab5OS_Ecriture_v0.8.25','Tab5OS_Ecriture_v0.8.26').replace('Tab5OS_Arduino_v0.8.25','Tab5OS_Arduino_v0.8.26'))
paths=[sketch/'Tab5Map.cpp',sketch/'Tab5Map.h',sketch/'Tab5MapSearchParser.h']
before={str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
result=subprocess.run(args+[str(sketch/'Tab5Map.cpp'),'-o',str(build/'Tab5Map.cpp.o')],capture_output=True,text=True,encoding='utf-8',errors='replace')
(build/'target-compile.log').write_text(result.stdout+result.stderr,encoding='utf-8')
su=build/'Tab5Map.cpp.su';frames=[]
if su.exists():
 for line in su.read_text(encoding='utf-8').splitlines():
  if any(token in line for token in ['8taskLoop','14downloadSearch','6decodeI']):frames.append(line)
unchanged=all(hashlib.sha256((release/p).read_bytes()).hexdigest()==value for p,value in before.items())
passed=result.returncode==0 and unchanged and len(frames)>=3
report={'version':'0.8.26','passed':passed,'physicalDeviceTested':False,'method':'Actual ESP32-P4 Clang20 compiler and M5Stack3.3.9 SDK using immutable .25 compile database with source/include paths replaced; -fstack-usage frames. Frames are individual functions, not complete TLS/LVGL call-chain bounds. Runtime stack checkpoints were added around search for device confirmation.','result':{'exitCode':result.returncode,'output':result.stdout+result.stderr,'sourceUnchanged':unchanged},'frames':frames,'sources':before,'tests':{str(Path(__file__).resolve().relative_to(release)):hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},'databaseSha256':hashlib.sha256(database_path.read_bytes()).hexdigest()}
(release/'tools/verification/maps-target-0826.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(result.stdout+result.stderr);print('PASS' if passed else 'FAIL',*frames,sep='\n')
raise SystemExit(0 if passed else 1)
