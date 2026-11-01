#!/usr/bin/env python3
"""Real JPEG encoder + exact export wrapper; real streaming writer and ABI6 queue."""
import argparse, hashlib, json, pathlib, subprocess, tempfile, zipfile
p=argparse.ArgumentParser();p.add_argument('--sketch',type=pathlib.Path,required=True);p.add_argument('--baseline-zip',type=pathlib.Path);p.add_argument('--report',type=pathlib.Path);a=p.parse_args()
here=pathlib.Path(__file__).parent
paths=[a.sketch/n for n in ['Tab5Jpeg.cpp','Tab5ImageWrite.h','Tab5AtomicFile.h','Tab5AppInputQueue.h','Tab5AppApi.h','src/jpeg/jpge.cpp','src/jpeg/jpge.h']]
paths += [here/'test_image_io.py',here/'image_io.cpp',here.parent/'atomic-storage-tests-0842/Platform.h',here.parent/'root-ui-tests-0839/image_write.cpp',here.parent/'abi6-tests-0832/main.cpp']
hashes={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
def read(name):
    if not a.baseline_zip:return (a.sketch/name).read_bytes()
    with zipfile.ZipFile(a.baseline_zip) as z:
        candidates=[n for n in z.namelist() if n.endswith('/'+name) and '/tools/' not in n]
        assert len(candidates)==1
        raw=z.read(candidates[0]);hashes[str(a.sketch/name)]=hashlib.sha256(raw).hexdigest();return raw
def run(cmd):
    r=subprocess.run(cmd,capture_output=True,text=True);print(r.stdout+r.stderr,end='');r.check_returncode();return r.stdout
with tempfile.TemporaryDirectory(prefix='tab5-image-io-') as temporary:
    folder=pathlib.Path(temporary)
    platform=(here.parent/'atomic-storage-tests-0842/Platform.h').read_text()
    platform=platform.replace('inline int failOperation=', 'inline bool flushFault=false,closeFault=false;\ninline int failOperation=')
    old=next(v for v in ['void flush(){operation();}','void flush(){if(!operation())error=true;}'] if v in platform)
    platform=platform.replace(old,'void flush(){openFailed=true;if(flushFault){errno=EIO;return;}operation();}')
    assert 'void close(){node.reset();}' in platform
    platform=platform.replace('void close(){node.reset();}', 'void close(){node.reset();if(closeFault)errno=EIO;}')
    platform=platform.replace('size_t read(uint8_t* out,size_t n)', 'bool seek(size_t at){if(!node||at>node->bytes.size()||!operation())return false;position=at;return true;}size_t read(uint8_t* out,size_t n)')
    platform=platform.replace('if(files.count(to)){errno=EEXIST;return false;}','')
    (folder/'Platform.h').write_text(platform)
    for name in ['Tab5VolumeFS.h','SD.h','Tab5Storage.h']:(folder/name).write_text('#include "Platform.h"\n')
    (folder/'Tab5AtomicFile.h').write_bytes((a.sketch/'Tab5AtomicFile.h').read_bytes())
    (folder/'Tab5ImageWrite.h').write_bytes(read('Tab5ImageWrite.h'))
    source=read('Tab5Jpeg.cpp').decode().replace('\r','');start=source.index('namespace {');end=source.index('\n#include <atomic>',start)
    (folder/'production.inc').write_text(source[start:end])
    flags=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(folder),'-I',str(a.sketch)]
    run(flags+[str(here/'image_io.cpp'),str(a.sketch/'src/jpeg/jpge.cpp'),'-o',str(folder/'images')])
    output=run([str(folder/'images')]+(['baseline'] if a.baseline_zip else []))
    if not a.baseline_zip:
        for name,path in [('writer',here.parent/'root-ui-tests-0839/image_write.cpp'),('abi6',here.parent/'abi6-tests-0832/main.cpp')]:
            run(flags+[str(path),'-o',str(folder/name)]);output+=run([str(folder/name)])
    if a.report:a.report.write_text(json.dumps({'version':'0.8.42','passed':True,'baseline':bool(a.baseline_zip),'physicalDeviceTested':False,'output':output.strip(),'sourceHashes':hashes,
        'scope':'Exact JPEG export wrapper with real jpge encoder, production ImageWrite template, existing full writer/ABI6 tests. Filesystem and capability allocator are doubles; errno-only flush failures mirror SDK behavior. No SD power-loss or target latency claim. ASan/UBSan, LSan disabled under executor ptrace.'},ensure_ascii=False,indent=2)+'\n')
