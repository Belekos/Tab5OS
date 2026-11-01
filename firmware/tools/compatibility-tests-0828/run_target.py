"""Compile the compatibility adapter and actual Wi-Fi hooks against target SDK."""
from pathlib import Path
import datetime,hashlib,json,subprocess
release=Path(__file__).resolve().parents[2]
sketch=release/'Tab5OS_Arduino_v0.8.28_Ecriture'
database_path=Path('C:/workspace/tab5-build-0827-clang/compile_commands.json')
database=json.loads(database_path.read_text(encoding='utf-8'))
template=next(row for row in database if Path(row['file']).name=='Tab5Tools.cpp')
args=[];skip=False
for arg in template['arguments']:
    if skip:skip=False;continue
    if arg=='-o':skip=True;continue
    if arg in ['-c','-MMD','-fstack-usage',template['file']]:continue
    args.append(arg.replace('Tab5OS_Ecriture_v0.8.27','Tab5OS_Ecriture_v0.8.28').replace('Tab5OS_Arduino_v0.8.27','Tab5OS_Arduino_v0.8.28'))
names=['Tab5Compatibility.cpp','Tab5Compatibility.h','Tab5CompatibilityPolicy.h','Tab5Audio.cpp','Tab5Audio.h']
hashes=lambda:{n:hashlib.sha256((sketch/n).read_bytes()).hexdigest() for n in names}
before=hashes();runs=[]
for name in ('Tab5Compatibility.cpp','Tab5Audio.cpp'):
    result=subprocess.run(args+['-fsyntax-only','-x','c++',str(sketch/name)],capture_output=True,encoding='utf-8',errors='replace')
    output=result.stdout+result.stderr
    runs.append(dict(name=name,exitCode=result.returncode,output=output))
    print(name,result.returncode);print(output,end='')
stable=before==hashes()
report=dict(version='0.8.28',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    method='ESP32-P4 target syntax compilation of production compatibility module and actual Tab5Audio worker hooks using installed M5Stack 3.3.9 ESP-IDF 5.5.4 SDK, ESP-Hosted 2.12.8 and M5GFX 0.2.30. No mocked target headers.',
    physicalDeviceTested=False,passed=stable and all(row['exitCode']==0 for row in runs),
    sourceStableDuringTests=stable,results=runs,sources=before,
    tests={str(Path(__file__).relative_to(release)).replace('\\','/'):hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
    database=str(database_path),hostedVersion='2.12.8',chipRevisionRange=[1,199])
(release/'tools/verification/compatibility-target-0828.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
