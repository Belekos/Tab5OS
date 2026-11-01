"""Production allocation branch with hostile allocator; native actual AES-GCM.

This verifies layout, wipe/OOM and format continuity, not target DMA operation.
"""
from pathlib import Path
import os,sys,subprocess,shutil,json,hashlib
here=Path(__file__).resolve().parent;release=here.parents[1];work=release.parent
build=Path('C:/workspace/tab5-vault-buffers-0828');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
mbed=work/'mbedtls-idf554-0822';objects=Path('C:/workspace/tab5-vault-reopen-0828/crypto-mt-asan')
assert (objects/'complete.json').exists(),'Run run_reopen.py first to build actual ASan mbedTLS'
rows=[]
for tag in ['0827','0828']:
    version='0.8.'+tag[-2:];sketch=work/('Tab5OS_Ecriture_v'+version)/('Tab5OS_Arduino_v'+version+'_Ecriture');out=build/tag;out.mkdir(exist_ok=True)
    names=['Tab5VaultCore.cpp','Tab5VaultCore.h'];snapshot={n:hashlib.sha256((sketch/n).read_bytes()).hexdigest() for n in names}
    for n in names:shutil.copy2(sketch/n,out/n)
    shutil.copy2(here/'buffers.cpp',out/'buffers.cpp')
    (out/'esp_heap_caps.h').write_text('#pragma once\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\ninline void* heap_caps_malloc(size_t bytes,int){return heap::allocate(4,bytes,true);}\ninline void* heap_caps_aligned_alloc(size_t alignment,size_t bytes,int){return heap::allocate(alignment,bytes,false);}\ninline void heap_caps_free(void* p){heap::release(p);}\n')
    (out/'sdkconfig.h').write_text('#pragma once\n#define CONFIG_CACHE_L2_CACHE_LINE_SIZE 128\n')
    cmd=['cl','/nologo','/MT','/W3','/utf-8','/EHsc','/std:c++17','/fsanitize=address','/Z7','/DARDUINO','/D_CRT_SECURE_NO_WARNINGS','/I'+str(out),'/I'+str(mbed/'include'),str(out/'buffers.cpp')]+[str(p) for p in objects.glob('*.obj')]+['/Fe:'+str(out/'test.exe'),'/link','/INCREMENTAL:NO','bcrypt.lib','ws2_32.lib','crypt32.lib','advapi32.lib']
    c=subprocess.run(cmd,cwd=out,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/'build.log').write_text(c.stdout+c.stderr,encoding='utf-8');print(c.stdout+c.stderr,flush=True);c.check_returncode()
    run=subprocess.run([str(out/'test.exe')],cwd=out,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=90);print(tag,run.returncode,run.stdout,run.stderr,flush=True)
    assert snapshot=={n:hashlib.sha256((sketch/n).read_bytes()).hexdigest() for n in names}
    rows.append(dict(version=version,exitCode=run.returncode,observations=json.loads(run.stdout),sources=snapshot))
passed=rows[0]['exitCode']==1 and rows[1]['exitCode']==0 and rows[0]['observations']['alignmentFailures']>0
report=dict(version='0.8.28',passed=passed,physicalDeviceTested=False,addressSanitizer=True,rows=rows,testSources={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [here/'buffers.cpp',Path(__file__)]},method='Actual VaultCore ARDUINO allocation branch, allocator forced to return ordinary PSRAM addresses aligned only to4bytes; alignment-aware allocator honors128bytes. Every size0..8193 layout/wipe/OOM; actual native mbedTLS3.6.5 AES-GCM encrypt/decrypt/tamper at15boundarysizes. Layout improvement is not claimed as the hardware reset cause: SDK5.5.4 already bounces unaligned DMA segments.')
(release/'tools/verification/vault-buffers-0828.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if passed else 1)
