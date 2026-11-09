"""Exercise serialized startup commands with FreeDOS, CuteMouse and FR/US keys."""
import argparse, hashlib, json, os, subprocess, sys, tempfile
from pathlib import Path
from make_examples import create

HERE=Path(__file__).resolve().parent
DOS=HERE.parent
SKETCH=DOS.parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output=a.output.resolve();a.output.parent.mkdir(parents=True,exist_ok=True)
if os.name=='nt' and not os.environ.get('VSCMD_VER'):
    batch=a.output.with_suffix('.cmd')
    batch.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n'+subprocess.list2cmdline([sys.executable,'-B',str(Path(__file__).resolve()),'--output',str(a.output)])+'\n')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(batch)]))

def function(text, signature):
    start=text.index(signature);end=text.index('{',start);depth=1
    while depth:
        end+=1
        if text[end]=='{':depth+=1
        elif text[end]=='}':depth-=1
    return text[start:end+1]

reports=[]
with tempfile.TemporaryDirectory(prefix='tab5-dos-startup-') as folder:
    work=Path(folder);examples=work/'program folder';create(examples)
    # The same real keyboard source, with its production TAB5_DOS mapping
    # enabled only in this translation unit (storage keeps its host adapter).
    keyboard=work/'keyboard.c'
    keyboard.write_text('#define TAB5_DOS 1\n'+(DOS/'core/ibmpc/keyboard.c').read_text(encoding='utf8'),encoding='utf8')
    main=work/'main.c';app=(DOS/'app.c').read_text(encoding='utf8')
    main.write_text('#include <stdint.h>\n#include <ctype.h>\nint dos_french_keyboard;\nint dos_key_held(uint16_t scan){(void)scan;return 0;}\n'+function(app,'uint16_t dos_ascii_key(')+'\n#include "'+(HERE/'startup_host.c').as_posix()+'"\n',encoding='utf8')
    host=work/('startup.exe' if os.name=='nt' else 'startup')
    command=[sys.executable,'-B',str(HERE/'build_host.py'),*(['--msvc'] if os.name=='nt' else []),'--main',str(main),'--keyboard-source',str(keyboard),'-o',str(host)]
    build=subprocess.run(command,cwd=work,capture_output=True,text=True,encoding='utf8',errors='replace')
    if build.returncode:print(build.stdout+build.stderr);build.check_returncode()
    for french,driver in ((0,1),(1,1),(0,0),(1,0)):
        run=subprocess.run([str(host),str(SKETCH/'sdcard/DOS/FREEDOS.IMG'),str(SKETCH/'sdcard/System/DOS/DRIVERS.IMG'),str(examples),str(french),str(driver)],cwd=work,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120)
        print(run.stdout+run.stderr,flush=True);run.check_returncode()
        reports.append({'french':bool(french),'mouse_driver':bool(driver),'passed':'STARTUP PASS' in run.stdout,'stdout':run.stdout})
sources=[DOS/'app.c',DOS/'startup.h',DOS/'core/ibmpc/keyboard.c',HERE/'startup_host.c',HERE/'build_host.py',Path(__file__)]
report={'passed':all(r['passed'] for r in reports),'scenarios':reports,'sourceHashes':{s.relative_to(SKETCH.parents[1]).as_posix():hashlib.sha256(s.read_bytes()).hexdigest() for s in sources},'limitations':['Real FreeDOS shell and serial mouse driver on host; not physical Tab5 timing.']}
a.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
