#!/usr/bin/env python3
"""Host checks of production header guard and exact pinned SDK CPU parsers."""
from pathlib import Path
import hashlib,io,json,os,re,subprocess,tempfile
from PIL import Image
root=Path(__file__).resolve().parents[2]
sketch=root/'Tab5OS_Arduino_v0.8.48_Ecriture'
here=Path(__file__).resolve().parent
build=Path(tempfile.mkdtemp(prefix='tab5-jpeg-0848-'))
def block(text,start):
    pos=text.index(start);brace=text.index('{',pos);depth=0
    for end in range(brace,len(text)):
        if text[end]=='{':depth+=1
        elif text[end]=='}':
            depth-=1
            if not depth:return text[pos:end+1]
    raise AssertionError(start)
source=(here/'sdk/jpeg_parse_marker.c').read_text()
private=(here/'sdk/jpeg_private.h').read_text()
struct_start=private.index('typedef struct {\n    uint8_t *buffer_offset;')
struct_end=private.index('} jpeg_dec_header_info_t;',struct_start)+len('} jpeg_dec_header_info_t;')
types=(here/'sdk/jpeg_types.h').read_text()
union_start=types.index('typedef union {')
union_end=types.index('} jpeg_huffman_table_info_t;',union_start)+len('} jpeg_huffman_table_info_t;')
native=types[union_start:union_end]+'\n'+private[struct_start:struct_end]+'\n'
for name in ['static uint8_t jpeg_get_char(', 'uint32_t jpeg_get_bytes(', 'esp_err_t jpeg_parse_dqt_marker(', 'esp_err_t jpeg_parse_sof_marker(', 'esp_err_t jpeg_parse_dht_marker(']:
    native+=block(source,name)+'\n'
native+=block((here/'sdk/jpeg_decode.c').read_text(),'esp_err_t jpeg_decoder_get_info(')
fixture=(here/'jpeg_probe_fixture.cpp').read_text().replace('// NATIVE_PARSERS',native)
(build/'test.cpp').write_text(fixture)
for index,(w,h) in enumerate([(8,8),(320,240),(1280,720),(17,16)]):
    for variant in range(4):
        im=Image.new('L' if variant==0 else 'RGB',(w,h),119 if variant==0 else (89,141,239))
        im.save(build/f'{index*4+variant}.jpg',format='JPEG',subsampling=max(0,variant-1),quality=80)
cmd=['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-Wno-sign-compare','-fno-pie','-no-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined','-I'+str(sketch),str(build/'test.cpp'),'-o',str(build/'test')]
subprocess.run(cmd,check=True)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
run=subprocess.run([str(build/'test'),str(build)],capture_output=True,text=True,timeout=60,env=env)
print(run.stdout+run.stderr,end='');run.check_returncode()
report={'version': '0.8.48', **json.loads(run.stdout)}
report['unsafeSdkReproductions']={}
for mode in ['unsafe-dqt','unsafe-dht','unsafe-gray']:
    failure=subprocess.run([str(build/'test'),str(build),mode],capture_output=True,text=True,timeout=10,env=env)
    expected=failure.returncode!=0 and ('runtime error:' in failure.stderr or 'AddressSanitizer' in failure.stderr)
    if not expected:raise AssertionError((mode,failure.returncode,failure.stdout,failure.stderr))
    report['unsafeSdkReproductions'][mode]={'detectedBySanitizer':True,'exitCode':failure.returncode}
    (root/'tools/verification'/f'video-jpeg-{mode}-0848.log').write_text(failure.stderr)
report['scope']='Exact production Tab5JpegHeader.h and SDK v5.5.4 CPU header parsers. SDK HAL/DMA/network not executed. SDK parsers and native structure extracted unchanged; allocator/log boundary instrumented. 16 Pillow baseline JPEGs, hostile headers and deterministic fuzz; ASan+UBSan. LeakSanitizer is unavailable in this runtime; explicit allocator ownership/count tracking verifies the reproduced get_info leak.'
report['sourceHashes']={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [sketch/'Tab5JpegHeader.h',here/'jpeg_probe.py',here/'jpeg_probe_fixture.cpp',*sorted((here/'sdk').glob('*'))]}
out=root/'tools/verification/video-jpeg-probe-0848.json';out.write_text(json.dumps(report,indent=2)+'\n');print(out)
