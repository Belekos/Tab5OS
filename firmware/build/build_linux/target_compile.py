#!/usr/bin/env python3
"""Compile live release units using the exact restored Arduino/P4 database."""
import argparse, hashlib, json, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--database',type=Path,required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--report',type=Path,required=True);p.add_argument('sources',nargs='+');a=p.parse_args()
from compile_linux import sketch_path
r=Path(__file__).resolve().parents[2];s=sketch_path(r)
# Report paths are release-relative regardless of the caller's working directory.
if not a.report.is_absolute():a.report=r/a.report
a.build.mkdir(parents=True,exist_ok=True);a.report.parent.mkdir(parents=True,exist_ok=True)
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
files=[s/name for name in a.sources];before={str(f.relative_to(r)):sha(f) for f in files};db=json.loads(a.database.read_text());results={}
for source in files:
 if source.suffix not in ('.cpp','.c'):continue
 entry=next((x for x in db if Path(x['file']).name==source.name),None)
 if entry is None:
  template='Tab5Memory.cpp' if source.suffix=='.cpp' else 'Tab5CryptoLockCompat.c'
  entry=next((x for x in db if Path(x['file']).name==template),None)
  if entry is None:raise SystemExit('Missing compile template: '+source.name)
 command=entry['arguments'][:];command=[str(source) if x==entry['file'] else x for x in command];command[command.index('-o')+1]=str(a.build.resolve()/(source.name+'.o'))
 result=subprocess.run(command,cwd=entry['directory'],capture_output=True,text=True);log=a.report.parent/(a.report.stem+'-'+source.name+'.log');log.write_text(result.stdout+result.stderr)
 if result.returncode: print(result.stdout+result.stderr);raise SystemExit(result.returncode)
 frames=[];stack=a.build/(source.name+'.su')
 if stack.exists():
  for line in stack.read_text().splitlines():
   parts=line.split('\t')
   if len(parts)>=3:frames.append({'function':parts[0].replace(str(s)+'/', ''),'bytes':int(parts[1]),'kind':parts[2]})
 results[source.name]={'compiled':True,'objectSha256':sha(a.build/(source.name+'.o')),'largestFrames':sorted(frames,key=lambda f:f['bytes'],reverse=True)[:8],'logSha256':sha(log)}
assert before=={str(f.relative_to(r)):sha(f) for f in files},'Production source changed during compilation'
compilerVersion=subprocess.check_output([command[0],'--version'],text=True).splitlines()[0]
import re
version=re.search(r'APP_VERSION\[\]\s*=\s*"([^"]+)"',(s/(s.name+'.ino')).read_text())[1]
report={'version':version,'passed':True,'physicalDeviceTested':False,'compiler':compilerVersion,'sourceHashes':before,'testSourceHashes':{str(Path(__file__).resolve().relative_to(r)):sha(Path(__file__))},'units':results,'limitations':['Target object compilation does not exercise hardware.','GCC per-function stack frames do not measure cumulative task stack usage.']}
a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'passed':True,'compiled':list(results),'report':str(a.report)}))
