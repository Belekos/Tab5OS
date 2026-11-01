"""Review actual vendored P4 objects. Direct-call examples are not stack bounds."""
from pathlib import Path
import hashlib,json,re,subprocess,sys
here=Path(__file__).resolve().parent;release=here.parents[1];work=release.parent
sketch=release/'Tab5OS_Arduino_v0.8.24_Ecriture';vendor=sketch/'src/tab5git'
build=Path(sys.argv[1]) if len(sys.argv)>1 else Path('C:/workspace/tab5-git-vendor-0824')
proof=release/'tools/verification/git-vendor-compile-0824.json'
compiled=json.loads(proof.read_text(encoding='utf-8'))
if not compiled.get('passed'):raise SystemExit('Current vendored P4 compilation must pass first.')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
if not compiled.get('sourcesStableDuringTest') or not compiled.get('inputs'):raise SystemExit('Compile proof must snapshot all C/header inputs before and after compilation.')
for name,digest in compiled['inputs'].items():
 if sha(release/name)!=digest:raise SystemExit('Stale compilation input: '+name)
flags=compiled.get('responseFlags','')
if '-fstack-usage' not in flags:raise SystemExit('Compile proof does not enable -fstack-usage.')
if hashlib.sha256(flags.encode()).hexdigest()!=compiled.get('responseSha256'):
 # Windows text mode normalizes CRLF; accept that precise source encoding too.
 if hashlib.sha256(flags.replace('\n','\r\n').encode()).hexdigest()!=compiled.get('responseSha256'):raise SystemExit('Response-file flags do not match recorded hash.')
objects=[];source_by_object={};empty_objects=set()
for row in compiled['results']:
 source=sketch/row['file']
 if row['exit'] or sha(source)!=row['sha256']:raise SystemExit('Stale or failed compilation: '+row['file'])
 obj=build/source.relative_to(vendor).with_suffix('.o')
 if not obj.is_file():raise SystemExit('Missing object: '+str(obj))
 if not obj.with_suffix('.su').is_file():
  nm=subprocess.run([str(Path(compiled['compiler']).with_name('llvm-nm.exe')),'--defined-only','--format=posix',str(obj)],capture_output=True,text=True);nm.check_returncode()
  if re.search(r'^\S+\s+[Tt]\s+',nm.stdout,re.M):raise SystemExit('Missing -fstack-usage for functions: '+str(obj))
  empty_objects.add(obj)
 objects.append(obj);source_by_object[obj.as_posix()]=row['file']
nodes={};byname={};edges={}
for obj in objects:
 if obj in empty_objects:continue
 for line in obj.with_suffix('.su').read_text().splitlines():
  fields=line.split('\t')
  if len(fields)!=3:continue
  name=fields[0].rsplit(':',1)[-1];key=(obj.as_posix(),name)
  nodes[key]=dict(source=source_by_object[obj.as_posix()],name=name,bytes=int(fields[1]),kind=fields[2])
  byname.setdefault(name,[]).append(key);edges[key]=set()
objdump=Path(compiled['compiler']).with_name('llvm-objdump.exe');unknown=set()
for i in range(0,len(objects),24):
 result=subprocess.run([str(objdump),'-r',*[p.as_posix() for p in objects[i:i+24]]],capture_output=True,text=True,encoding='utf-8');result.check_returncode()
 obj=None;caller=None
 for line in result.stdout.splitlines():
  match=re.match(r'(.+):\s+file format ',line)
  if match:obj=Path(match[1]).as_posix();continue
  match=re.match(r'RELOCATION RECORDS FOR \[\.text\.(.+)\]:',line)
  if match:caller=(obj,match[1]);continue
  if line.startswith('RELOCATION RECORDS'):caller=None
  match=re.match(r'\S+\s+R_RISCV_CALL(?:_PLT)?\s+(\S+)',line)
  if match and caller in nodes:
   target=match[1];same=(obj,target);options=[same] if same in nodes else byname.get(target,[])
   if len(options)==1:edges[caller].add(options[0])
   else:unknown.add(target)
cycles=set();memo={}
def visit(key,seen):
 if key in seen:cycles.add(key);return (0,[])
 # Cached acyclic path examples may not cross the caller path.
 if key in memo and not seen.intersection(memo[key][1]):return memo[key]
 best=(0,[])
 for target in sorted(edges[key]):
  value=visit(target,seen|{key})
  if value[0]>best[0]:best=value
 answer=(nodes[key]['bytes']+best[0],[key]+best[1]);memo[key]=answer;return answer
entries=[]
for entry in ['git_libgit2_init','git_repository_open_ext','git_clone','git_remote_fetch','git_remote_push','git_index_add_all','git_commit_create','git_diff_index_to_workdir','git_status_foreach_ext','git_regexp_compile','git_regexp_match']:
 if len(byname.get(entry,[]))!=1:raise SystemExit('Missing or ambiguous entry: '+entry)
 size,path=visit(byname[entry][0],set());entries.append(dict(entry=entry,frameSum=size,path=[nodes[key] for key in path]))
header=(sketch/'Tab5Git.h').read_text(encoding='utf-8')
match=re.search(r'WorkerStackBytes\s*=\s*(\d+)u?\s*\*\s*1024u?',header)
if not match:raise SystemExit('Unrecognized worker stack constant')
worker_stack=int(match[1])*1024
rows=sorted(nodes.values(),key=lambda row:(-row['bytes'],row['source'],row['name']))
def frame(name):
 values=[row['bytes'] for row in rows if row['name']==name]
 if len(values)!=1:raise SystemExit('Missing or ambiguous frame: '+name)
 return values[0]
checks={'vendorCompileCurrent':True,'workerStackAtLeast40KiB':worker_stack>=40*1024,
        'pcreCompileInitialFrameAtMost4KiB':frame('pcre2_compile_8')<=4096,
        'hashFdFrameAtMost8KiB':frame('git_odb__hashfd')<=8192,
        'terminalUsesIsolatedWorker':'Tab5Git::runIsolated(' in (sketch/'Tab5Terminal.cpp').read_text(encoding='utf-8')}
tracked=sorted(p for p in vendor.rglob('*') if p.is_file() and p.suffix in ('.c','.h','.inc'))
tracked += [sketch/n for n in ['Tab5Git.cpp','Tab5Git.h','Tab5GitPort.cpp','Tab5GitPort.h','Tab5GitView.cpp','Tab5Terminal.cpp','Tab5Task.h']]
report=dict(version='0.8.24',passed=all(checks.values()),physicalDeviceTested=False,wholeStackBound=False,
 method='Per-function -fstack-usage from current vendored ESP32-P4 objects plus direct RISC-V CALL relocations. Frame sums are explicit acyclic path examples, not proven whole-stack maxima or free-stack margins. Recursion is cut; callbacks, SDK, indirect calls and exception paths are unresolved; tail calls may be overcounted.',
 workerStackBytes=worker_stack,checks=checks,rows=rows,entries=entries,unresolved=sorted(unknown),cycles=[nodes[k] for k in sorted(cycles)],
 sources={p.relative_to(sketch).as_posix():sha(p) for p in tracked},tests={Path(__file__).name:sha(Path(__file__))},
 compiler=compiled['compiler'],compileReportSha256=sha(proof),compileInputs=compiled['inputs'],responseSha256=compiled['responseSha256'],responseFlags=flags,
 objectInputs={p.relative_to(build).as_posix():{'objectSha256':sha(p),'stackUsageSha256':None if p in empty_objects else sha(p.with_suffix('.su')),'noDefinedTextSymbols':p in empty_objects} for p in objects})
(release/'tools/verification/git-stack-0824.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
for row in entries:print(row['entry'],row['frameSum'],' -> '.join(x['name']+'('+str(x['bytes'])+')' for x in row['path']))
print('PASS' if report['passed'] else 'FAIL','frames',len(rows),'cycles',len(cycles),'unresolved',len(unknown),'configured worker',worker_stack,'wholeStackBound=false')
raise SystemExit(0 if report['passed'] else 1)
