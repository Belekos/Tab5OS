#!/usr/bin/env python3
"""Compare fast video I/O with the original dispatcher and real controllers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--cc',default=os.environ.get('CC','cl' if os.name=='nt' else 'cc'))
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
repo=root.parents[4]
with tempfile.TemporaryDirectory(prefix='tab5-dos-io-') as temporary:
    work=Path(temporary)
    exe=work/('io_dispatch.exe' if os.name=='nt' else 'io_dispatch')
    command=[sys.executable,str(root/'tests/build_host.py')]
    command+=['--msvc'] if Path(a.cc).stem.lower()=='cl' else ['--cc',a.cc]
    command+=['--main',str(root/'tests/test_io_dispatch.c'),'-o',str(exe)]
    result=subprocess.run(command,text=True,encoding='utf-8',errors='replace',capture_output=True)
    if result.returncode:
        sys.stderr.write(result.stdout+result.stderr)
        raise SystemExit(result.returncode)
    result=subprocess.run([str(exe)],text=True,encoding='utf-8',errors='replace',capture_output=True,timeout=120)
    if result.returncode:
        sys.stderr.write(result.stdout+result.stderr)
        raise SystemExit(result.returncode)
    report=json.loads(result.stdout.strip().splitlines()[-1])
    paths=list((root/'core').rglob('*.c'))+list((root/'core').rglob('*.h'))
    paths += [root/'x87_disabled.c',root/'tests/build_host.py',root/'tests/test_io_dispatch.c',Path(__file__).resolve()]
    paths += list((root/'tests/host-compat').rglob('*'))
    report['sourceHashes']={path.relative_to(repo).as_posix():hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(set(paths)) if path.is_file()}
    report['limitations']=['Host correctness regression only. No Tab5 speed or FPS gain is inferred.']
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({key:value for key,value in report.items() if key!='sourceHashes'},indent=2))
