"""Boot real FreeDOS and execute original COM/MZ fixtures with adjacent data."""
from pathlib import Path
import argparse,hashlib,json,subprocess,tempfile,time
from make_examples import create
p=argparse.ArgumentParser();p.add_argument('--host',type=Path,required=True);p.add_argument('--image',type=Path,required=True);p.add_argument('--output',type=Path);a=p.parse_args()
expected='8c25e36d0a72a77a8c3541ba12f4dd5f4b2dbb12c15e660cc18d7da545020688'
image=a.image.resolve();before=hashlib.sha256(image.read_bytes()).hexdigest();assert before==expected,'Unexpected FreeDOS base image'
with tempfile.TemporaryDirectory(prefix='tab5-dos-') as temp:
 folder=Path(temp)/'DOS sample folder with spaces';create(folder)
 fixture_hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir()}
 (folder/'HELLO.EXE').rename(folder/'Hello long name.EXE')
 started=time.monotonic();run=subprocess.run([str(a.host.resolve()),str(image),str(folder),'Hello long name.EXE'],capture_output=True,text=True,timeout=120)
 combined=run.stdout+'\n'+run.stderr
 checks={name:name in combined for name in ['FreeDOS kernel 2043','FreeCom version 0.86','TAB5OS DOS READY','TAB5 COM OK','TAB5 MZ EXE OK','TAB5 ADJACENT DATA OK','HELLOL~1.EXE','FRENCH DRIVER INSTALLED']}
 checks['french_azerty_scancodes']=any(line.strip()=='azerty' for line in run.stdout.splitlines())
 checks['base_image_unchanged']=hashlib.sha256(image.read_bytes()).hexdigest()==before
 report={'schema':1,'passed':run.returncode==0 and all(checks.values()),'runtime':'real 80286 PC/AT + FreeDOS 1.4','freedos_image':{'path':'sdcard/DOS/FREEDOS.IMG','sha256':before,'bytes':image.stat().st_size,'kernel':'2043','shell':'0.86a'},'checks':checks,'seconds':round(time.monotonic()-started,3),'returncode':run.returncode,'stdout':run.stdout,'stderr':run.stderr}
 root=Path(__file__).resolve().parents[6];source=Path(__file__).resolve().parents[1]
 report['sourceHashes']={p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(source.rglob('*')) if p.is_file() and p.suffix in ('.c','.h','.py')}
 report['fixtureHashes']=fixture_hashes
 report['hostExecutable']={'sha256':hashlib.sha256(a.host.read_bytes()).hexdigest(),'bytes':a.host.stat().st_size}
 if a.output:a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:v for k,v in report.items() if k not in ('stdout','stderr')},indent=2))
 if not report['passed']:print(combined);raise SystemExit(1)
