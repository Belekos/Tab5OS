#!/usr/bin/env python3
"""Run the production wrapper against the pinned IDF cache API with instrumented HAL."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;release=here.parents[1]
sketch=next(release.glob('Tab5OS_Arduino_v*_Ecriture'))
build=a.build.resolve();build.mkdir(parents=True,exist_ok=True)
stubs=build/'stubs';stubs.mkdir(exist_ok=True)
empty=['sys/lock.h','sdkconfig.h','esp_check.h','esp_log.h','freertos/FreeRTOS.h',
       'freertos/portmacro.h','esp_heap_caps.h','esp_rom_caps.h','soc/soc_caps.h',
       'hal/mmu_hal.h','hal/cache_hal.h','hal/cache_ll.h','esp_compiler.h',
       'esp_private/esp_cache_private.h','esp_private/critical_section.h',
       'esp_attr.h','esp_idf_version.h','esp_memory_utils.h']
for name in empty:
    out=stubs/name;out.parent.mkdir(parents=True,exist_ok=True);out.write_text('/* Instrumented declarations come from TestCache.h. */\n')
(stubs/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_ERR_INVALID_ARG 0x102\n')
(stubs/'esp_bit_defs.h').write_text('#pragma once\n#define BIT(n) (1U<<(n))\n')
native=here/'sdk/esp_cache_msync.c'
source=native.read_text();cut=source.index('//The esp_cache_aligned_malloc function')
# Compile the complete cache API and its helpers verbatim; the unrelated
# deprecated allocation helpers below this marker are not part of this test.
(build/'native.c').write_text('#define esp_cache_msync tab5_native_cache_msync\n'+source[:cut])
common=['-g','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',
        '-I'+str(stubs),'-I'+str(here),'-I'+str(here/'sdk'),'-include',str(here/'TestCache.h')]
commands=[['gcc','-std=c11',*common,'-Wno-pointer-to-int-cast','-Wno-unused-variable','-c',str(build/'native.c'),'-o',str(build/'native.o')],
          ['g++','-std=c++17',*common,'-c',str(sketch/'Tab5CacheSync.cpp'),'-o',str(build/'wrapper.o')],
          ['g++','-std=c++17',*common,str(here/'cache_sync.cpp'),str(build/'native.o'),str(build/'wrapper.o'),'-o',str(build/'test')]]
logs=[]
for command in commands:
    result=subprocess.run(command,text=True,capture_output=True);logs.append(result.stdout+result.stderr)
    if result.returncode:print(logs[-1]);raise SystemExit(result.returncode)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
result=subprocess.run([str(build/'test')],text=True,capture_output=True,env=env);logs.append(result.stdout+result.stderr)
(build/'run.log').write_text('\n'.join(logs));print(logs[-1],end='')
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
inputs=[sketch/'Tab5CacheSync.cpp',here/'TestCache.h',here/'cache_sync.cpp',Path(__file__),native,here/'sdk/esp_cache.h']
report={'passed':result.returncode==0,'physicalDeviceTested':False,'nativeSource':'ESP-IDF v5.5.4 components/esp_mm/esp_cache_msync.c',
        'sourceHashes':{str(x.relative_to(release)):sha(x) for x in inputs},'commands':commands,'result':logs[-1],
        'limitations':['HAL and FreeRTOS boundaries are instrumented on host; no physical cache or IRQ latency measured.',
                      'ASan/UBSan enabled; leak sanitizer disabled because unsupported under executor ptrace.',
                      'ISR, critical-context, instruction, invalid/unknown flags and unaligned combined invalidation retain native calls.']}
(build/'result.json').write_text(json.dumps(report,indent=2)+'\n')
raise SystemExit(result.returncode)
