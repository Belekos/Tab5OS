"""Compile series units with a real previously provisioned ESP32-P4 SDK DB."""
from pathlib import Path
import argparse,json,subprocess,hashlib,datetime
p=argparse.ArgumentParser();p.add_argument('--database',type=Path,default=Path('C:/workspace/tab5-build-0825-clang/compile_commands.json'));p.add_argument('--output',type=Path,default=Path('C:/workspace/tab5-serial-0827'));args=p.parse_args()
root=Path(__file__).resolve().parents[2];sketch=root/'Tab5OS_Arduino_v0.8.27_Ecriture'
db=json.loads(args.database.read_text(encoding='utf-8'));template=next(row for row in db if Path(row['file']).name=='Tab5Tools.cpp')
rows=[];args.output.mkdir(exist_ok=True,parents=True)
for name in ['Tab5Serial.cpp','Tab5SerialCommand.cpp','Tab5Writing.cpp','Tab5Terminal.cpp','Tab5OS_Arduino_v0.8.27_Ecriture.ino']:
 command=[];skip=False
 for arg in template['arguments']:
  if skip:skip=False;continue
  if arg=='-o':skip=True;continue
  if arg in ['-c','-MMD',template['file']]:continue
  command.append(arg.replace('Tab5OS_Ecriture_v0.8.25','Tab5OS_Ecriture_v0.8.27').replace('Tab5OS_Arduino_v0.8.25','Tab5OS_Arduino_v0.8.27'))
 source=sketch/name;target=args.output/(name+'.o');before=hashlib.sha256(source.read_bytes()).hexdigest()
 result=subprocess.run(command+['-c','-x','c++',str(source),'-o',str(target)],capture_output=True,text=True,encoding='utf-8',errors='replace')
 stack=target.with_suffix('.su');stackText=stack.read_text(encoding='utf-8') if stack.is_file() else ''
 row=dict(name=name,sha256=before,sourceStable=before==hashlib.sha256(source.read_bytes()).hexdigest(),exitCode=result.returncode,output=result.stdout+result.stderr,stackUsage=stackText);rows.append(row);print(name,result.returncode,row['output'],flush=True)
headers=['Tab5Serial.h','Tab5SerialCore.h','Tab5SerialSettings.inc','Tab5SerialText.h','Tab5WritingSerial.inc'];hashes={name:hashlib.sha256((sketch/name).read_bytes()).hexdigest() for name in headers}
report=dict(version='0.8.27',method='Real ESP32-P4 Clang compile with stack usage, installed SDK',physicalDeviceTested=False,checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),database=str(args.database),passed=all(row['exitCode']==0 and row['sourceStable'] for row in rows),files=rows,headers=hashes,sourceHashes={str(Path(__file__).resolve().relative_to(root)).replace('\\','/'):hashlib.sha256(Path(__file__).read_bytes()).hexdigest()})
(root/'tools/verification/serial-syntax-0827.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
