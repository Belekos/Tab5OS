#!/usr/bin/env python3
"""Compile audio owner and prepared Speaker against the exact restored P4 SDK."""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--database',required=True,type=Path);p.add_argument('--sdk',required=True,type=Path);p.add_argument('--work',required=True,type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2];sketch=root/'Tab5OS_Arduino_v0.8.49_Ecriture';a.work.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(root/'tools/build_linux'))
from prepare_m5unified_audio import prepare_m5unified_audio
patched=prepare_m5unified_audio(a.sdk,a.work)
db=json.loads(a.database.read_text());results=[]
for name in ['M5Unified.cpp','Tab5AudioDriver.cpp','Tab5Audio.cpp']:
 entry=next(e for e in db if e['file'].endswith('/'+name));args=entry['arguments'][:]
 oldm5=next((x[2:] for x in args if x.startswith('-I') and x.endswith('/M5Unified/src')),None)
 if oldm5:args=[x.replace(str(Path(oldm5).parent),str(patched)) for x in args]
 source=patched/'src/M5Unified.cpp' if name=='M5Unified.cpp' else sketch/name
 if name!='M5Unified.cpp':args=[str(source) if x==entry['file'] else x for x in args]
 args[args.index('-o')+1]=str(a.work/(name+'.o'))
 args=[x for x in args if x!='-w']+['-Wall','-Wextra','-Werror=return-type','-Werror=uninitialized','-Werror=maybe-uninitialized','-Werror=use-after-free']
 before=hashlib.sha256(source.read_bytes()).hexdigest();r=subprocess.run(args,cwd=entry['directory'],capture_output=True,text=True);log=a.work/(name+'.log');log.write_text(r.stdout+r.stderr)
 assert before==hashlib.sha256(source.read_bytes()).hexdigest(),'Source changed during compilation'
 results.append({'source':name,'compilerExit':r.returncode,'sourceSha256':before,'arguments':args,'logSha256':hashlib.sha256(log.read_bytes()).hexdigest(),'warningCount':r.stderr.count('warning:')})
 print(name,r.returncode)
 if r.returncode:print(r.stderr)
 r.check_returncode()
report={'passed':True,'scope':'Exact restored P4 SDK compilation of M5Unified, Tab5AudioDriver and Tab5Audio. Not a firmware link or physical audio test.','results':results,'patch':json.loads((a.work/'m5unified-audio-patch.json').read_text())}
(root/'tools/audit-0849/audio-target-tests.json').write_text(json.dumps(report,indent=2)+'\n')
