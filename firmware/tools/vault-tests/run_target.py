from pathlib import Path
import hashlib,json,subprocess
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.25_Ecriture';build=Path('C:/workspace/tab5-vault-0825-target');build.mkdir(exist_ok=True)
database=Path('C:/workspace/tab5-build-0824-clang/compile_commands.json');template=next(r for r in json.loads(database.read_text()) if Path(r['file']).name=='Tab5Tools.cpp')
args=[];skip=False
for arg in template['arguments']:
 if skip:skip=False;continue
 if arg=='-o':skip=True;continue
 if arg in ['-c','-MMD','-fstack-usage',template['file']]:continue
 args.append(arg.replace('Tab5OS_Ecriture_v0.8.24','Tab5OS_Ecriture_v0.8.25').replace('Tab5OS_Arduino_v0.8.24','Tab5OS_Arduino_v0.8.25'))
rows=[]
for name in ['Tab5KeyStore.cpp','Tab5Vault.cpp','Tab5VaultCore.cpp','Tab5Secrets.cpp']:
 output=build/(name+'.o');r=subprocess.run(args+['-c','-fstack-usage','-o',str(output),str(sketch/name)],capture_output=True,text=True,encoding='utf-8',errors='replace');frames=[]
 if not r.returncode:
  for line in output.with_suffix('.su').read_text().splitlines():
   fields=line.split('\t')
   if len(fields)==3:frames.append(dict(function=fields[0],bytes=int(fields[1]),kind=fields[2]))
  frames.sort(key=lambda f:f['bytes'],reverse=True)
 rows.append(dict(source=name,exitCode=r.returncode,output=r.stdout+r.stderr,frames=frames));print(name,r.returncode,frames[:3],r.stdout+r.stderr,flush=True)
names=[p.name for p in sketch.glob('Tab5Vault*')]+['Tab5KeyStore.cpp','Tab5KeyStore.h','Tab5Secrets.cpp','Tab5Secrets.h']
report=dict(version='0.8.25',passed=all(r['exitCode']==0 for r in rows),physicalDeviceTested=False,method='ESP32-P4 target Clang and installed M5Stack SDK, per-function frames only; platform crypto/NVS callchain and physical high-water marks are not measured.',rows=rows,sources={n:hashlib.sha256((sketch/n).read_bytes()).hexdigest() for n in names},tests={here.name+'/run_target.py':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()})
(release/'tools/verification/vault-target-0825.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
