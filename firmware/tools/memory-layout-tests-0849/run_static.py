#!/usr/bin/env python3
"""Check static SRAM reductions without weakening SHA-1 or XZ validation."""
import hashlib,json,lzma,os,pathlib,random,subprocess,tempfile
here=pathlib.Path(__file__).resolve().parent
root=here.parents[1];sketch=root/'Tab5OS_Arduino_v0.8.41_Ecriture'
sha=sketch/'src/tab5git/src/util/hash/sha1dc';xz=sketch/'src/xz'
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
fixtures={'sha-mbles-1.bin':'3ead211681cec93d265c8ac123dd062e105408cebf82fa6e2b126f4f40bcb88c',
          'sha-mbles-2.bin':'208feafe1c6a95c73f662514ac48761f25e1f3b74922521a98d9ce287f4a2197'}
with tempfile.TemporaryDirectory(prefix='tab5-static-memory-0841-') as d:
    build=pathlib.Path(d);exe=build/'static-data'
    sources=[here/'static_data.c',sha/'sha1.c',sha/'ubc_check.c',
             *[xz/f for f in ('xz_crc32.c','xz_crc64.c','xz_dec_lzma2.c','xz_dec_stream.c')]]
    subprocess.run(['gcc','-std=gnu17','-O1','-g','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie',
        '-DSHA1DC_FORCE_ALIGNED_ACCESS=1',
        '-DSHA1DC_CUSTOM_INCLUDE_SHA1_C="stdint.h"','-DSHA1DC_CUSTOM_INCLUDE_UBC_CHECK_C="stdint.h"',
        '-include','stdlib.h','-include','string.h',
        '-I',str(sha),'-I',str(xz),*[str(f) for f in sources],'-o',str(exe)],check=True)
    def call(*args):
        return subprocess.check_output([str(exe),*map(str,args)],env=env,text=True).strip()
    print(call())
    random_data=random.Random(841).randbytes(32769)
    for data in (b'',b'abc',b'a'*63,b'a'*64,b'a'*65,random_data):
        file=build/'plain';file.write_bytes(data)
        for chunk in (1,63,64,257,4096):
            assert call('sha1',file,chunk,1)=='0 '+hashlib.sha1(data).hexdigest()
    collision_hashes=[]
    for name,digest in fixtures.items():
        file=here/'fixtures'/name;data=file.read_bytes()
        assert hashlib.sha256(data).hexdigest()==digest
        collision_hashes.append(hashlib.sha1(data).hexdigest())
        for chunk in (1,64,257,4096):
            assert call('sha1',file,chunk,0)=='1 '+hashlib.sha1(data).hexdigest()
            detected,safe_hash=call('sha1',file,chunk,1).split()
            assert detected=='1' and safe_hash!=hashlib.sha1(data).hexdigest()
    assert collision_hashes[0]==collision_hashes[1]
    print('SHA-1: 30 standard/chunked cases; 16 collision/safe-hash cases: pass')
    for check in (lzma.CHECK_CRC32,lzma.CHECK_CRC64):
        data=b'Tab5 XZ verification\n'*16384+random_data
        encoded=build/'input.xz';decoded=build/'decoded';encoded.write_bytes(lzma.compress(data,check=check))
        call('xz',encoded,len(data)+1,decoded)
        assert decoded.read_bytes()==data
    print('Production XZ decoder: CRC32 and CRC64 full round trips: pass')
    symbols=subprocess.check_output(['nm','-S',str(exe)],text=True)
    assert any(' R sha1_dvs' in line for line in symbols.splitlines())
    print('Host sha1_dvs symbol is read-only; ESP32-P4 ELF verifies final SRAM gain')
    inputs=[*sources,sha/'ubc_check.h',xz/'xz_crc32_table.h',xz/'xz_crc64_table.h',
            here/'run_static.py',*[here/'fixtures'/f for f in fixtures]]
    report={'version':'0.8.41','passed':True,'physicalDeviceTested':False,
            'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],
            'standardSha1Cases':30,'collisionSha1Cases':16,'crcTableEntriesChecked':512,
            'xzRoundTrips':['CRC32','CRC64'],'hostSha1VectorsReadOnly':True,
            'scope':'Production SHA1DC and XZ sources; aligned input path matching RISC-V; independent CRC table generation and Python hashlib/lzma oracles',
            'limits':['Final SRAM section placement must be verified separately in the ESP32-P4 ELF','No physical timing, cache or peripheral validation'],
            'sourceHashes':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs}}
    out=root/'tools/verification/memory-static-0841.json';out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(report,indent=2)+'\n');print(out)
