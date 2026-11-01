#!/usr/bin/env python3
"""Regenerate Arduino main source, then compile it with the real target SDK."""
import argparse,hashlib,json,re,subprocess,sys
from pathlib import Path
from compile_linux import sketch_path
p=argparse.ArgumentParser();p.add_argument('--environment',type=Path,required=True);a=p.parse_args();e=a.environment.resolve();r=Path(__file__).resolve().parents[2];s=sketch_path(r);main=s/(s.name+'.ino')
version=re.search(r'APP_VERSION\[\]\s*=\s*"([^"]+)"',main.read_text())[1];tag=version.replace('.','');out=r/('tools/verification/main-target-'+tag);out.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
before=sha(main);command=[sys.executable,str(Path(__file__).with_name('compile_linux.py')),'--environment',str(e),'--release-root',str(r),'--database-only']
with (out/'generate.log').open('w') as log:result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
if result.returncode:print((out/'generate.log').read_text()[-8000:]);raise SystemExit(result.returncode)
db=json.loads((e/'target-build/compile_commands.json').read_text());entry=next(x for x in db if Path(x['file']).name==s.name+'.ino.cpp');command=entry['arguments'][:];command[command.index('-o')+1]=str((e/('target-build/main-target-'+tag+'.o')).resolve())
with (out/'compile.log').open('w') as log:result=subprocess.run(command,cwd=entry['directory'],stdout=log,stderr=subprocess.STDOUT)
if result.returncode:print((out/'compile.log').read_text()[-12000:]);raise SystemExit(result.returncode)
assert before==sha(main),'Main changed during compilation'
report={'version':version,'passed':True,'physicalDeviceTested':False,'sourceHashes':{str(main.relative_to(r)):before},'testSourceHashes':{str(Path(__file__).resolve().relative_to(r)):sha(Path(__file__))},'evidenceHashes':{str(f.relative_to(r)):sha(f) for f in out.iterdir()},'limitations':['Target object compilation; final full link and physical tests remain separate.']};(out.parent/('main-target-'+tag+'.json')).write_text(json.dumps(report,indent=2)+'\n');print('PASS regenerated main compiled with target SDK')
