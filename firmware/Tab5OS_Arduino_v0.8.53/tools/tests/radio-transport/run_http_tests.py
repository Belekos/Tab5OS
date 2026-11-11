#!/usr/bin/env python3
"""Compile the actual radio HTTP source against fragmented transport fixtures."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

HERE=Path(__file__).resolve().parent
FIRMWARE=HERE.parents[2]
REPOSITORY=FIRMWARE.parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--cxx',default=os.environ.get('CXX','cl' if os.name=='nt' else 'c++'))
a=p.parse_args()
msvc=Path(a.cxx).stem.lower()=='cl'
source_path=FIRMWARE/'Tab5Audio.cpp'
source=source_path.read_text(encoding='utf8')
start=source.index('class Tab5HttpAudioSource')
snippet=source[start:source.index('// One producer and one consumer',start)]
buffered=source[source.index('class Tab5BufferedRadioSource'):]
def extract(signature):
    start=buffered.index(signature)
    end=buffered.index('{',start)
    depth=1
    while depth:
        end+=1
        if buffered[end]=='{': depth+=1
        elif buffered[end]=='}': depth-=1
    return buffered[start:end+1]+'\n'
with tempfile.TemporaryDirectory(prefix='tab5-radio-http-') as temporary:
    work=Path(temporary)
    (work/'http_source.inc').write_text(snippet,encoding='utf8')
    (work/'prefix_source.inc').write_text(''.join(extract(signature) for signature in
        ('uint32_t read(void*', 'uint32_t readNonBlock(void*', 'uint32_t readPrefix(void*')),encoding='utf8')
    exe=work/('radio_http.exe' if os.name=='nt' else 'radio_http')
    if msvc:
        cmd=[a.cxx,'/nologo','/std:c++17','/EHsc','/W4','/WX','/D_CRT_SECURE_NO_WARNINGS',
             '/I'+str(FIRMWARE),'/I'+str(work),str(HERE/'test_http.cpp'),'/Fe:'+str(exe)]
    else:
        cmd=[a.cxx,'-std=c++17','-Wall','-Wextra','-Werror','-g','-O1',
             '-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(FIRMWARE),
             '-I'+str(work),str(HERE/'test_http.cpp'),'-o',str(exe)]
    subprocess.run(cmd,cwd=work,check=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
    result=subprocess.run([str(exe)],cwd=work,env=env,text=True,capture_output=True)
    print(result.stdout,end='')
    print(result.stderr,end='')
    result.check_returncode()
    count=re.search(r'RADIO HTTP PASS checks=(\d+):',result.stdout)
    if not count: raise SystemExit('Missing radio HTTP success marker')
paths=[source_path,FIRMWARE/'Tab5Audio.h',FIRMWARE/'Tab5RadioBodyReader.h',
       FIRMWARE/'Tab5RadioCodec.h',HERE/'test_http.cpp',Path(__file__)]
report={'component':'radio-http-source','passed':True,'checks':int(count[1]),
        'sanitizers':[] if msvc else ['AddressSanitizer','UndefinedBehaviorSanitizer'],
        'stdout':result.stdout,'sourceHashes':{q.relative_to(REPOSITORY).as_posix():hashlib.sha256(q.read_bytes()).hexdigest() for q in paths},
        'limitations':['Actual HTTP source and framing code with deterministic socket/HTTPClient fixtures; physical C6/TLS transport and reset behaviour need device validation.']}
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
