#!/usr/bin/env python3
"""Read target sizeof from P4 ELF symbols without pretending to measure live heaps."""
from pathlib import Path
import argparse,hashlib,json,subprocess
p=argparse.ArgumentParser();p.add_argument('--database',required=True,type=Path);p.add_argument('--build',required=True,type=Path);a=p.parse_args()
r=Path(__file__).resolve().parents[2];s=r/'Tab5OS_Arduino_v0.8.47_Ecriture';a.build.mkdir(parents=True,exist_ok=True)
row=next(x for x in json.loads(a.database.read_text()) if Path(x['file']).name=='Tab5ResourceManager.cpp')
results={};commands={};sources={}
for label,header in [('baseline',r/'tools/memory-tests-0847/baseline-0846/Tab5ResourceManager.h'),('current',s/'Tab5ResourceManager.h')]:
 src=a.build/(label+'.cpp');obj=a.build/(label+'.o');src.write_text('#include "'+str(header)+'"\n#define private public\n#include "'+str(s/'WriterSyntaxIndex.h')+'"\n#undef private\nchar manager_bytes[sizeof(Tab5Resources::Manager)];\nchar lexical_checkpoint_bytes[sizeof(writing::SyntaxIndex::Checkpoint)*514];\n')
 args=[str(src) if x==row['file'] else x for x in row['arguments']]
 for i,x in enumerate(args):
  if x=='-o':args[i+1]=str(obj)
 subprocess.run(args,cwd=row['directory'],check=True,capture_output=True)
 nm=Path(args[0]).with_name('riscv32-esp-elf-nm');out=subprocess.check_output([str(nm),'-S',str(obj)],text=True)
 results[label]={line.split()[3]:int(line.split()[1],16) for line in out.splitlines() if line.endswith(('manager_bytes','lexical_checkpoint_bytes'))}
 commands[label]=args;sources[str(header.relative_to(r))]=hashlib.sha256(header.read_bytes()).hexdigest()
for name in ['Tab5ResourcePolicy.h','WriterSyntaxIndex.h','WriterSyntax.h']:
 f=s/name;sources[str(f.relative_to(r))]=hashlib.sha256(f.read_bytes()).hexdigest()
report={'version':'0.8.47','passed':True,'target':'ESP32-P4 prev3 GCC14.2 pinned','method':__doc__,'results':results,'sourceHashes':sources,'commands':commands,'compilerSha256':hashlib.sha256(Path(row['arguments'][0]).read_bytes()).hexdigest(),'testSourceSha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'limits':['Declared native structure sizes, not runtime free heap, CPU time or stack high water.','Lexical buffers are optional PSRAM arrays; two buffers release this size only if both were allocated.']}
(r/'tools/verification/resource-sizes-0847.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(results))
