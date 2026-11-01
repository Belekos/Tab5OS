#!/usr/bin/env python3
"""Compile the current Mesh sources with the restored real ESP32-P4 SDK."""
from pathlib import Path
import argparse,hashlib,json,subprocess
p=argparse.ArgumentParser();p.add_argument('--database',required=True,type=Path);p.add_argument('--build',required=True,type=Path);a=p.parse_args();r=Path(__file__).resolve().parent.parent;s=r/'Tab5OS_Arduino_v0.8.41_Ecriture';w=a.build.resolve();w.mkdir(parents=True,exist_ok=True);e=r/'tools/verification/meshtastic-target-0841';e.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
paths=[s/n for n in ['Tab5Meshtastic.cpp','Tab5Meshtastic.h','Tab5MeshtasticProtocol.h','Tab5MeshModel.h','Tab5MeshMemory.h','Tab5MeshAllocator.c','Tab5MeshAllocator.h','Tab5MeshAllocatorEspIdf554.inc','Tab5MeshCodec.h','Tab5MeshStore.cpp','Tab5MeshStore.h','Tab5Task.h','Tab5ResourceManager.h','Tab5ResourcePolicy.h']];before={str(x.relative_to(r)):sha(x) for x in paths};results={};configs={}
for item in json.loads(a.database.read_text()):
 name=Path(item['file']).name
 if name not in ['Tab5Meshtastic.cpp','Tab5MeshStore.cpp','Tab5CryptoLockCompat.c']:continue
 if name=='Tab5CryptoLockCompat.c':name='Tab5MeshAllocator.c'
 args=item['arguments'][:];args=[str(s/name) if x==item['file'] else x for x in args];args[args.index('-o')+1]=str(w/(name+'.o'));command=subprocess.run(args,cwd=item['directory'],capture_output=True,text=True);(e/(name+'.log')).write_text(command.stdout+command.stderr)
 if command.returncode:print(command.stdout+command.stderr);raise SystemExit(command.returncode)
 stack=w/(name+'.su');frames=[]
 if stack.exists():
  (e/stack.name).write_bytes(stack.read_bytes())
  for line in stack.read_text().splitlines():
   parts=line.split('\t')
   if len(parts)>=3:frames.append({'function':parts[0].replace(str(s)+'/', ''),'bytes':int(parts[1]),'kind':parts[2]})
 results[name]={'compiled':True,'largestFrames':sorted(frames,key=lambda x:x['bytes'],reverse=True)[:8]}
 compiler=Path(args[0]);version=subprocess.check_output([str(compiler),'--version'],text=True).splitlines()[0]
 for arg in args:
  if arg.startswith('-I') and arg.endswith('/dio_qspi/include'):
   config=Path(arg[2:])/'sdkconfig.h';configs[str(config)]={'sha256':sha(config),'nimbleDefines':[x for x in config.read_text().splitlines() if x.startswith('#define CONFIG_BT_NIMBLE_') and any(y in x for y in ['MEM_ALLOC_MODE','MSYS_','TRANSPORT_','HOST_TASK_STACK_SIZE','NVS_PERSIST','MAX_BONDS','ATT_PREFERRED_MTU'])]}
assert len(results)==3,results
assert before=={str(x.relative_to(r)):sha(x) for x in paths},'production changed during target compilation'
report={'version':'0.8.41','passed':True,'physicalDeviceTested':False,'compiler':version,'sourceHashes':before,'testSourceHashes':{str(Path(__file__).resolve().relative_to(r)):sha(Path(__file__))},'results':results,'sdkConfig':configs,'evidenceHashes':{str(x.relative_to(r)):sha(x) for x in sorted(e.iterdir()) if x.is_file()},'limitations':['Per-function GCC stack usage is not a bound on cumulative stack depth or SDK calls.','Real target compilation does not exercise radio interoperability or hosted controller startup/shutdown.']}
(r/'tools/verification/meshtastic-target-0841.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'passed':True,'compiled':list(results),'compiler':version}))
