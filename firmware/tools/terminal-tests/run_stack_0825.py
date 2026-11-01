"""Recompile current shell for P4 and derive the reviewed bounded recursive subtotal.

This is intentionally not a runtime stack watermark or a whole-program bound.
"""
from pathlib import Path
import datetime,hashlib,json,re,subprocess,sys
here=Path(__file__).resolve().parent;release=here.parents[1];sketch=release/'Tab5OS_Arduino_v0.8.25_Ecriture'
build=Path('C:/workspace/tab5-terminal-stack-0825');build.mkdir(exist_ok=True)
dbpath=Path(sys.argv[1] if len(sys.argv)>1 else 'C:/workspace/tab5-build-0823-clang/compile_commands.json')
database=json.loads(dbpath.read_text());template=next(x for x in database if Path(x['file']).name=='Tab5Shell.cpp')
args=[];skip=False
for arg in template['arguments']:
 if skip:skip=False;continue
 if arg=='-o':skip=True;continue
 if arg in ['-c','-MMD','-fstack-usage',template['file']]:continue
 args.append(re.sub(r'Tab5OS_(Ecriture|Arduino)_v0\.8\.\d+',r'Tab5OS_\1_v0.8.25',arg))
source=sketch/'Tab5Shell.cpp';obj=build/'Tab5Shell.cpp.o';command=args+['-c','-fstack-usage','-o',str(obj),str(source)]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
names=['Tab5Shell.cpp','Tab5Shell.h','Tab5ShellBuffer.h','Tab5FilePolicy.h','Tab5TerminalPath.h','Tab5ShellHelp.h','Tab5Terminal.cpp','Tab5Terminal.h','Tab5TerminalBuffer.h']
before={n:digest(sketch/n) for n in names}
r=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace');print(r.stdout+r.stderr)
if r.returncode:raise SystemExit(r.returncode)
frames=[]
for line in obj.with_suffix('.su').read_text().splitlines():
 fields=line.split('\t')
 if len(fields)==3:
  symbol=fields[0].split(':')[-1];frames.append(dict(symbol=symbol,bytes=int(fields[1]),kind=fields[2]))
cxxfilt=Path(args[0]).with_name('llvm-cxxfilt.exe');demangled=subprocess.run([str(cxxfilt)],input='\n'.join(x['symbol'] for x in frames),capture_output=True,text=True,check=True).stdout.splitlines()
for row,name in zip(frames,demangled):row['function']=name
sizes={}
for row in frames:
 match=re.match(r'Tab5Shell::Engine::(\w+)\(',row['function'])
 if match:sizes[match[1]]=max(sizes.get(match[1],0),row['bytes'])
required=['run','block','pipeline','command','expand','substitute','execute','script','builtin','visit']
assert all(x in sizes for x in required),sizes
shell=source.read_text(encoding='utf-8');terminal=(sketch/'Tab5Terminal.cpp').read_text(encoding='utf-8')
assert 'if(++depth_>6)' in shell and 'if(depth_>4+substitutions_)' in shell and 'if(++substitutions_>4)' in shell and 'if(depth>16)' in shell
stack=int(re.search(r'StackBytes=(\d+)',terminal)[1]);assert stack==16384
# Per nested invocation: block -> pipeline -> command, then either
# expand -> substitute -> block, or execute -> script -> block.
# Loops recurse directly through block. Charging each permitted block a full
# command/pipeline frame is conservative for loops and preserves the prior
# reviewed subtotal. The final builtin and execute remain present for visit.
paths=[]
for substitutions in range(5):
 for scripts in range(4):
  blocks=1+substitutions+scripts
  if blocks>6 or blocks>4+substitutions:continue
  total=sizes['run']+blocks*(sizes['block']+sizes['pipeline']+sizes['command'])+substitutions*(sizes['expand']+sizes['substitute'])+scripts*(sizes['execute']+sizes['script'])+sizes['execute']+sizes['builtin']
  paths.append(dict(scripts=scripts,substitutions=substitutions,blocks=blocks,knownShellBytes=total))
worst=max(paths,key=lambda p:p['knownShellBytes'])
# Includes the depth-17 rejected call in addition to depths 0..16.
recursive_directory=18*sizes['visit'];known=worst['knownShellBytes']+recursive_directory;remaining=stack-known
inputs={n:before[n] for n in names[:6]}
report=dict(version='0.8.25',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=r.returncode==0 and all(x['kind']=='static' for x in frames) and remaining>=2048,physicalDeviceTested=False,sourceStableDuringTests=before=={n:digest(sketch/n) for n in names},source='Tab5Shell.cpp',sourceSha256=before['Tab5Shell.cpp'],sources=before,compilationInputs=inputs,method='Current P4 Clang object -fstack-usage; bounded reviewed shell/directory recursive subtotal from current frame sizes. Not a whole-program maximum or physical watermark.',compileDatabase=str(dbpath),compileDatabaseSha256=digest(dbpath),command=command,frames=frames,workerStackBytes=stack,maximumBlocks=6,maximumSubstitutions=4,maximumNonSubstitutionBlocks=4,worstKnownShellPath=worst,rejectedDirectoryDepthIncluded=True,directoryRecursionBytes=recursive_directory,knownShellAndDirectoryBytes=known,remainingBeforePlatformFrames=remaining,componentFrameBytes=sizes,pathsReviewed=paths,tests={'tools/terminal-tests/run_stack.py':digest(Path(__file__))},scope='Source control-flow review plus current P4 static frames. libc, filesystem, network/TLS, callbacks, worker wrapper and interrupt/task frames are excluded from this subtotal and require runtime watermark measurements. No device used.')
(release/'tools/verification/terminal-stack-0825.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('shell/directory bytes',known,'remaining before platform frames',remaining)
raise SystemExit(0 if report['passed'] and report['sourceStableDuringTests'] else 1)
