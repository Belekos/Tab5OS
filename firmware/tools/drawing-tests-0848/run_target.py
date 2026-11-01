#!/usr/bin/env python3
from pathlib import Path
import argparse,json,subprocess,hashlib
p=argparse.ArgumentParser();p.add_argument('--db',required=True,type=Path);p.add_argument('--build',required=True,type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;release=here.parents[1];sketch=release/'Tab5OS_Arduino_v0.8.48_Ecriture';work=a.build.resolve();work.mkdir(parents=True,exist_ok=True);db=json.loads(a.db.read_text());proof=release/'tools/verification/drawing-target-0848';proof.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def command(name):return next(c for c in db if Path(c['file']).name==name)
results={};sources={};generated={}
def compile(name,source,key):
 c=command(name);args=list(c['arguments']);args[args.index(c['file'])]=str(source);args[args.index('-o')+1]=str(work/(key+'.o'));args=[x for x in args if x!='-w'];r=subprocess.run(args,cwd=c['directory'],capture_output=True,text=True);(proof/(key+'.log')).write_text(r.stdout+r.stderr);results[key]={'returncode':r.returncode,'diagnostics':r.stdout+r.stderr};r.check_returncode();
 if source.is_relative_to(release):sources[str(source.relative_to(release))]=sha(source)
 else:generated[source.name]={'sha256':sha(source),'contents':source.read_text()}
 return work/(key+'.o')
for label,src in [('baseline',here/'baseline'),('current',sketch/'tools/native-apps/image-editor')]:
 compile('Tab5ImageCore.c',src/'image_core.c',label+'-core');layout=work/(label+'-layout.c');layout.write_text('#include "'+str(src/'image_core.h')+'"\nchar state_size[sizeof(IEState)];\nchar doc_size[sizeof(IEDoc)];\n');obj=compile('Tab5ImageCore.c',layout,label+'-layout');nm=Path(command('Tab5ImageCore.c')['arguments'][0]).with_name('riscv32-esp-elf-nm');out=subprocess.run([str(nm),'-S','--radix=d',str(obj)],capture_output=True,text=True,check=True).stdout;results[label+'-layout']['sizes']={line.split()[-1]:int(line.split()[1]) for line in out.splitlines() if line.split()[-1] in ('state_size','doc_size')};usage=(work/(label+'-core.su')).read_text();(proof/(label+'-stack.su')).write_text(usage);results[label+'-core']['stateSwapStack']=[l for l in usage.splitlines() if ':state_swap\t' in l]
compile('Tab5ImageEditor.cpp',sketch/'Tab5ImageEditor.cpp','current-editor')
for p in [Path(__file__).resolve(),sketch/'tools/native-apps/image-editor/image_core.h',sketch/'tools/native-apps/image-editor/image_selection.h',here/'baseline/image_core.h',here/'baseline/image_selection.h']:sources[str(p.relative_to(release))]=sha(p)
report={'generatedLayoutInputs':generated,'version':'0.8.48','passed':True,'method':'Replay pinned target compile database against actual source; target object symbols determine structure sizes. No device execution.','sourceHashes':sources,'results':results,'compileDatabaseSha256':sha(a.db)}
(proof.parent/'drawing-target-0848.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(results,indent=2))
