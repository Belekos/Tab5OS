"""Compile and execute the production floppy overlay with OS callback mocks."""
from pathlib import Path
import argparse,hashlib,json,subprocess,time,os
p=argparse.ArgumentParser();p.add_argument('--cc',default='cc');p.add_argument('--msvc',action='store_true');p.add_argument('--output',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];repo=r.parents[4];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);exe=out/('dos-overlay.exe' if os.name=='nt' else 'dos-overlay')
include=[r,r.parents[2],r/'core/ibmpc',r/'core/x86',r/'core/upd765',r/'core/diskspace',r/'core/emudbg']
sources=[r/'overlay.c',r/'tests/test_overlay.c'];host=r/'tests/host-compat/platform.h'
cmd=(['cl','/nologo','/std:c11','/O2','/D_CRT_SECURE_NO_WARNINGS','/DEMUDBG_OFF','/FI'+str(host)] if a.msvc else [a.cc,'-std=gnu11','-O1','-g','-DEMUDBG_OFF','-fsanitize=undefined','-fno-sanitize-recover=all','-include',str(host)])
for path in include:cmd += [('/I' if a.msvc else '-I'),str(path)]
cmd += [str(s) for s in sources]+(['/Fe:'+str(exe)] if a.msvc else ['-o',str(exe)])
subprocess.run(cmd,check=True,cwd=out);started=time.monotonic();run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
manifest=sources+[r/'platform.h',r/'tests/run_overlay_tests.py',host]
report={'schema':1,'passed':run.returncode==0,'seconds':round(time.monotonic()-started,3),'stdout':run.stdout,'stderr':run.stderr,'sourceHashes':{p.relative_to(repo).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in manifest}}
(out/'dos-overlay-tests.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(run.stdout);print(run.stderr);raise SystemExit(run.returncode)
