"""Compile serial/editor units against the real ESP32-P4 SDK, including recovery."""
from pathlib import Path
import argparse,json,subprocess,hashlib,datetime
p=argparse.ArgumentParser()
p.add_argument('--database',type=Path,default=Path('C:/workspace/tab5-build-0827-clang/compile_commands.json'))
p.add_argument('--output',type=Path,default=Path('C:/workspace/tab5-serial-0828'))
args=p.parse_args()
root=Path(__file__).resolve().parents[2];sketch=root/'Tab5OS_Arduino_v0.8.28_Ecriture'
db=json.loads(args.database.read_text(encoding='utf-8'))
template=next(row for row in db if Path(row['file']).name=='Tab5Tools.cpp')
units=['Tab5Serial.cpp','Tab5SerialCommand.cpp','Tab5Writing.cpp','WriterStorage.cpp','Tab5Terminal.cpp','Tab5OS_Arduino_v0.8.28_Ecriture.ino']
headers=['Tab5Serial.h','Tab5SerialCore.h','Tab5SerialSettings.inc','Tab5SerialText.h','Tab5WritingSerial.inc','Tab5Writing.h','WriterStorage.h']
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
before={name:sha(sketch/name) for name in units+headers}
rows=[];args.output.mkdir(exist_ok=True,parents=True)
for name in units:
 command=[];skip=False
 for arg in template['arguments']:
  if skip:skip=False;continue
  if arg=='-o':skip=True;continue
  if arg in ['-c','-MMD','-fstack-usage',template['file']]:continue
  command.append(arg.replace('Tab5OS_Ecriture_v0.8.27','Tab5OS_Ecriture_v0.8.28').replace('Tab5OS_Arduino_v0.8.27','Tab5OS_Arduino_v0.8.28'))
 source=sketch/name;target=args.output/(name+'.o')
 result=subprocess.run(command+['-fstack-usage','-c','-x','c++',str(source),'-o',str(target)],capture_output=True,text=True,encoding='utf-8',errors='replace')
 stack=target.with_suffix('.su');stackText=stack.read_text(encoding='utf-8') if stack.is_file() and result.returncode==0 else ''
 row=dict(name=name,sha256=before[name],sourceStable=before[name]==sha(source),exitCode=result.returncode,output=result.stdout+result.stderr,stackUsage=stackText)
 rows.append(row);print(name,result.returncode,row['output'],flush=True)
stable=before=={name:sha(sketch/name) for name in before}
report=dict(version='0.8.28',method='Real ESP32-P4 Clang compile with stack usage, installed SDK; serial, active editor and recovery SaveJob',physicalDeviceTested=False,sourceStableDuringTests=stable,checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),database=str(args.database),passed=stable and all(row['exitCode']==0 and row['sourceStable'] and row['stackUsage'] for row in rows),files=rows,headers={name:before[name] for name in headers},sourceHashes={str(Path(__file__).resolve().relative_to(root)).replace('\\','/'):sha(Path(__file__))})
(root/'tools/verification/serial-syntax-0828.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
