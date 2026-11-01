#!/usr/bin/env python3
"""Compile pristine and patched exact M5Unified with target warning checks."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--database',type=Path,required=True);p.add_argument('--sdk',type=Path,required=True);p.add_argument('--work',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[2];sys.path.insert(0,str(r/'tools/build_linux'))
from prepare_m5unified_audio import prepare_m5unified_audio
patched=prepare_m5unified_audio(a.sdk,a.work)
entry=next(x for x in json.loads(a.database.read_text()) if x['file'].endswith('/M5Unified.cpp'))
old=str(Path(entry['file']).parents[1]);results={}
warnings=['-Wall','-Wextra','-Werror=return-type','-Werror=uninitialized','-Werror=maybe-uninitialized','-Werror=use-after-free']
files=['src/M5Unified.cpp','src/M5Unified.inl','src/utility/Speaker_Class.hpp','src/utility/Speaker_Class.inl']
for label,source in [('before',a.sdk.resolve()),('after',patched)]:
 args=[x.replace(old,str(source)) for x in entry['arguments'] if x!='-w']+warnings
 args[args.index('-o')+1]=str(a.work.resolve()/(label+'.o'))
 hashes={name:hashlib.sha256((source/name).read_bytes()).hexdigest() for name in files}
 proc=subprocess.run(args,cwd=entry['directory'],capture_output=True,text=True)
 log=proc.stdout+proc.stderr;path=r/'tools/verification'/('audio-sdk-target-0848-'+label+'.log');path.write_text(log)
 assert hashes=={name:hashlib.sha256((source/name).read_bytes()).hexdigest() for name in files},'Source changed during compile'
 results[label]={'compilerExit':proc.returncode,'source':str(source),'hashes':hashes,'warningCount':len(re.findall(r'warning:',log)),'warnings':sorted(set(re.findall(r'warning: (.*)',log))),'log':str(path.relative_to(r)),'logSha256':hashlib.sha256(path.read_bytes()).hexdigest(),'arguments':args}
 if proc.returncode:print(log)
 proc.check_returncode()
report={'version':'0.8.48','passed':True,'physicalDeviceTested':False,'sourceHashes':{str(q.relative_to(r)):hashlib.sha256(q.read_bytes()).hexdigest() for q in [r/'tools/build_linux/prepare_m5unified_audio.py',Path(__file__)]},'scope':'Exact restored P4 target compile of pristine and isolated patched M5Unified, warning suppression removed; no hardware test.',**results,'addedWarningMessages':sorted(set(results['after']['warnings'])-set(results['before']['warnings'])),'testHashes':{str(Path(__file__).relative_to(r)):hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}}
(r/'tools/verification/audio-sdk-target-0848.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'passed':True,'beforeWarnings':results['before']['warningCount'],'afterWarnings':results['after']['warningCount'],'addedWarningMessages':report['addedWarningMessages']}))
