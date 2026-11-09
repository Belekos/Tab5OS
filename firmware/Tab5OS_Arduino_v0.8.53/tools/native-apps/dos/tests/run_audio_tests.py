#!/usr/bin/env python3
"""Build and run the real sound core and the native audio-output regressions."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path)
p.add_argument('--cc',default=os.environ.get('CC','cl' if os.name=='nt' else 'cc'))
a=p.parse_args()
r=Path(__file__).resolve().parents[1]
msvc=Path(a.cc).stem.lower()=='cl'

def run(command,cwd):
    result=subprocess.run(command,cwd=cwd,text=True,encoding='utf-8',errors='replace',capture_output=True)
    if result.returncode:
        sys.stderr.write(result.stdout+result.stderr)
        raise SystemExit(result.returncode)
    return result.stdout

with tempfile.TemporaryDirectory(prefix='tab5-audio-tests-') as temporary:
    work=Path(temporary)
    core=work/('audio_core.exe' if os.name=='nt' else 'audio_core')
    output=work/('audio_output.exe' if os.name=='nt' else 'audio_output')
    run([sys.executable,str(r/'tests/build_host.py'),*(['--msvc'] if msvc else ['--cc',a.cc]),'--main',str(r/'tests/audio_host.c'),'-o',str(core)],work)
    flags=['/nologo','/std:c11','/O2','/DPC_AUDIO_HZ=22050'] if msvc else ['-std=c11','-O2','-DPC_AUDIO_HZ=22050']
    for component in ('ibmpc','x86'):
        flags += [('/I' if msvc else '-I')+str(r/'core'/component)]
    run([a.cc,*flags,str(r/'tests/audio_output_host.c'),*(['/Fe:'+str(output)] if msvc else ['-o',str(output)])],work)
    core_log=run([str(core)],work)
    output_log=run([str(output)],work)
    match=re.search(r'AUDIO PASS checks=(\d+) opl_hash=([0-9a-f]+) synth_ms_per_second=([0-9.]+)',core_log)
    if not match or 'AUDIO OUTPUT PASS:' not in output_log:
        raise SystemExit('Audio test did not produce its success marker')
    paths=['build.py','app_audio.h','core/ibmpc/pc.h','core/ibmpc/speaker.c','core/ibmpc/pit.c','core/ibmpc/sound.c',
           'core/emu8950/emu8950.c','core/emu8950/emu8950.h','core/emu8950/emuadpcm.c','tests/build_host.py',
           'tests/audio_host.c','tests/audio_output_host.c','tests/run_audio_tests.py']
    report={'passed':True,'sample_rate':22050,'core_checks':int(match[1]),'opl_pcm_hash':match[2],
            'host_synth_ms_per_emulated_second':float(match[3]),'benchmark_scope':'host only, 9 OPL2 voices; not Tab5 FPS',
            'audio_output_passed':True,'core_stdout':core_log,'audio_output_stdout':output_log,
            'source_sha256':{name:hashlib.sha256((r/name).read_bytes()).hexdigest() for name in paths}}
    if a.output:
        a.output.parent.mkdir(parents=True,exist_ok=True)
        a.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(core_log+output_log,end='')
