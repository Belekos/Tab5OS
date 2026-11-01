#!/usr/bin/env python3
"""Compile selected current units with Arduino warning suppression removed.

A separate diagnostic pass, not the firmware build and not a runtime test.
Uses the exact build database, ABI, optimization, SDK and dependency paths.
"""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
from compile_linux import sketch_path,manifest
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--environment',type=Path,required=True)
p.add_argument('--database',type=Path,required=True)
p.add_argument('sources',nargs='+')
a=p.parse_args();r=Path(__file__).resolve().parents[2];s=sketch_path(r);e=a.environment.resolve();out=e/'warning-pass';out.mkdir(exist_ok=True)
version=re.search(r'APP_VERSION\[\]\s*=\s*"([^"]+)"',(s/(s.name+'.ino')).read_text())[1]
db=json.loads(a.database.read_text());before=manifest(s);results={}
flags=['-Wall','-Wextra','-Werror=return-type','-Werror=uninitialized','-Werror=maybe-uninitialized','-Werror=use-after-free']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for name in a.sources:
 source=s/name
 template=name
 if name.endswith('.ino'):
  template=s.name+'.ino.cpp';source=e/'build/sketch'/template
  if not source.is_file():raise ValueError('Run after the complete current firmware build')
 entry=next((v for v in db if Path(v['file']).name==template),None)
 if not entry:raise ValueError('Source absent from live Arduino database: '+name)
 command=[str(source) if v==entry['file'] else v for v in entry['arguments'] if v!='-w'];command.extend(flags)
 command[command.index('-o')+1]=str(out/(name+'.o'))
 run=subprocess.run(command,cwd=entry['directory'],capture_output=True,text=True);logs=e/'qa/warning-pass';logs.mkdir(parents=True,exist_ok=True);log=logs/(name+'.log');log.write_text(run.stdout+run.stderr)
 warnings=[line for line in (run.stdout+run.stderr).splitlines() if re.search(r'\bwarning:',line)]
 errors=[line for line in (run.stdout+run.stderr).splitlines() if re.search(r'\berror:',line)]
 results[name]={'exitCode':run.returncode,'warningCount':len(warnings),'warningLines':warnings,'errorLines':errors,'logSha256':sha(log),'command':command}
 if run.returncode:print(run.stdout+run.stderr)
after=manifest(s);assert before==after,'Production sources changed during warning pass'
report={'version':version,'passed':all(v['exitCode']==0 for v in results.values()),'kind':'compiler-warning-pass','physicalDeviceTested':False,'suppressionRemoved':'-w','addedFlags':flags,'units':results,'sourceHashes':{str((s/name).relative_to(r)):sha(s/name) for name in a.sources},'scriptSha256':sha(Path(__file__)),'limitations':['A successful compile or warning pass does not prove functional correctness, lifetime safety or hardware behavior.','Vendor headers may emit existing warnings; all warning lines are retained, never hidden.','Only selected units were compiled with these diagnostic flags; the delivered firmware keeps the pinned normal build options.']}
qa=e/'qa';qa.mkdir(exist_ok=True);(qa/'compiler-warning-pass.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'passed':report['passed'],'units':len(results),'warnings':sum(v['warningCount'] for v in results.values()),'report':str(qa/'compiler-warning-pass.json')}))
raise SystemExit(0 if report['passed'] else 1)
