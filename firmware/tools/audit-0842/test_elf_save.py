#!/usr/bin/env python3
import argparse
import hashlib
import json
import pathlib
import subprocess
import tempfile
import zipfile

p=argparse.ArgumentParser()
p.add_argument('--sketch', type=pathlib.Path, required=True)
p.add_argument('--baseline-zip', type=pathlib.Path)
p.add_argument('--report', type=pathlib.Path)
a=p.parse_args()
here=pathlib.Path(__file__).parent
paths=[a.sketch/'Tab5Executable.cpp', a.sketch/'Tab5AtomicFile.h', here/'elf_save.cpp', here/'test_elf_save.py', here.parent/'atomic-storage-tests-0842/Platform.h']
raw=(a.sketch/'Tab5Executable.cpp').read_bytes()
if a.baseline_zip:
    with zipfile.ZipFile(a.baseline_zip) as z:
        candidates=[n for n in z.namelist() if n.endswith('/Tab5Executable.cpp') and '/tools/' not in n]
        assert len(candidates)==1
        raw=z.read(candidates[0])
source=raw.decode().replace('\r','')
def function(signature):
    start=source.index(signature); opening=source.index('{',start); depth=1; pos=opening+1
    while depth:
        depth+=(source[pos]=='{')-(source[pos]=='}');pos+=1
    return source[start:pos]
with tempfile.TemporaryDirectory(prefix='tab5-save-') as temporary:
    folder=pathlib.Path(temporary)
    platform=paths[-1].read_text()
    # Arduino File::flush ignores the return values of fflush/fsync and does
    # not set the Stream write-error flag. Emulate that actual SDK boundary.
    platform=platform.replace('inline int failOperation=', 'inline bool flushFault=false,closeFault=false;\ninline int failOperation=')
    old_flush=next(s for s in ['void flush(){if(!operation())error=true;}', 'void flush(){operation();}'] if s in platform)
    platform=platform.replace(old_flush, 'void flush(){if(flushFault){errno=EIO;return;}operation();}')
    assert 'void close(){node.reset();}' in platform
    platform=platform.replace('void close(){node.reset();}', 'void close(){node.reset();if(closeFault)errno=EIO;}')
    # POSIX-style destination replacement exposes the data-loss danger when
    # exists() falsely reports absence; stricter FAT rename is not a guarantee.
    platform=platform.replace('if(files.count(to)){errno=EEXIST;return false;}','')
    (folder/'Platform.h').write_text(platform)
    for name in ['Tab5VolumeFS.h','SD.h','Tab5Storage.h']:(folder/name).write_text('#include "Platform.h"\n')
    (folder/'Tab5AtomicFile.h').write_bytes((a.sketch/'Tab5AtomicFile.h').read_bytes())
    (folder/'production.inc').write_text(function('bool validPath(')+'\n'+function('int appSave('))
    binary=folder/'test'
    subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(folder),str(here/'elf_save.cpp'),'-o',str(binary)],check=True)
    run=subprocess.run([str(binary)]+(['baseline'] if a.baseline_zip else []),capture_output=True,text=True)
    print(run.stdout,end='')
    print(run.stderr,end='')
    run.check_returncode()
    if a.report:
        hashes={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
        hashes[str(a.sketch/'Tab5Executable.cpp')]=hashlib.sha256(raw).hexdigest()
        a.report.write_text(json.dumps({'passed':True,'baseline':bool(a.baseline_zip),'physicalDeviceTested':False,
            'output':run.stdout.strip(),'sourceHashes':hashes,
            'scope':'Exact appSave and path validator plus production AtomicFile helper; injected filesystem errors, flush EIO without Stream write-error flag (SDK behavior), and POSIX-style rename replacement. No physical card or power-loss durability claim. ASan/UBSan; LSan unavailable under executor ptrace.'},indent=2)+'\n')
