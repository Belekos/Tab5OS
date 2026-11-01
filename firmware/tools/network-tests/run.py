"""Execute actual parser and network source with deterministic platform stubs."""
from pathlib import Path
import argparse, hashlib, json, shutil, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('sketch',type=Path);p.add_argument('--cxx',default='cl');a=p.parse_args()
here=Path(__file__).resolve().parent
sketch=a.sketch.resolve()
with tempfile.TemporaryDirectory(prefix='tab5-network-') as name:
    d=Path(name)
    # Copy source byte-for-byte so quote includes use the simulated platform.
    source=sketch/'Tab5NetworkTools.cpp'
    shutil.copy2(source,d/source.name)
    for filename in ['Platform.h','protocol.cpp','session.cpp','arguments.cpp']:
        shutil.copy2(here/filename,d/filename)
    for header in ['Tab5NetworkGate.h','Tab5Audio.h','Arduino.h','WiFi.h','esp_random.h',
                   'lwip/dns.h','lwip/tcpip.h','lwip/sockets.h','lwip/inet.h','fcntl.h','unistd.h']:
        path=d/header;path.parent.mkdir(exist_ok=True)
        path.write_text('#include "Platform.h"\n',encoding='utf-8')
    for test in ['protocol','session','arguments']:
        subprocess.run([a.cxx,'/nologo','/std:c++17','/EHsc','/W4','/utf-8','/I'+str(d),'/I'+str(sketch),test+'.cpp','/Fe:'+test+'.exe'],cwd=d,check=True)
        subprocess.run([str(d/(test+'.exe'))],cwd=d,check=True)
    print('Production source SHA256:',hashlib.sha256(source.read_bytes()).hexdigest())
