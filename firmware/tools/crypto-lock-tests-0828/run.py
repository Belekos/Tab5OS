"""Actual SDK/compat lock source with only OS locks and chip caps simulated."""
from pathlib import Path
import datetime,hashlib,json,os,subprocess,sys
here=Path(__file__).resolve().parent;release=here.parents[1]
sketch=release/'Tab5OS_Arduino_v0.8.28_Ecriture'
build=Path('C:/workspace/tab5-crypto-lock-0828');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    cmd=build/'run.cmd'
    cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
stubs=build/'stubs';(stubs/'sys').mkdir(parents=True,exist_ok=True);(stubs/'hal').mkdir(exist_ok=True)
(stubs/'sys/lock.h').write_text('#pragma once\n#include <mutex>\nstruct _lock_t { std::mutex mutex; };\nvoid _lock_acquire(_lock_t*);void _lock_release(_lock_t*);\n')
(stubs/'sdkconfig.h').write_text('#pragma once\n#ifndef TEST_OTHER_TARGET\n#define CONFIG_IDF_TARGET_ESP32P4 1\n#endif\n')
(stubs/'esp_idf_version.h').write_text('#pragma once\n#define ESP_IDF_VERSION_VAL(a,b,c) (((a)<<16)|((b)<<8)|(c))\n#ifdef TEST_FUTURE_SDK\n#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5,5,5)\n#else\n#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5,5,4)\n#endif\n')
(stubs/'hal/ecdsa_ll.h').write_text('#pragma once\nbool ecdsa_ll_is_mpi_required();\n')
names=['hmac','ds','sha_aes','dma','mpi','ecc','ecdsa','key_manager']
caps=['DIG_SIGN_SUPPORTED','HMAC_SUPPORTED','MPI_SUPPORTED','SHA_SUPPORTED','AES_SUPPORTED','ECC_SUPPORTED','ECDSA_SUPPORTED','ECDSA_USES_MPI','KEY_MANAGER_SUPPORTED','SHA_CRYPTO_DMA','AES_CRYPTO_DMA']
(stubs/'esp_crypto_lock.h').write_text('#pragma once\n'+''.join('#define SOC_'+c+' 1\n' for c in caps)+''.join('void esp_crypto_'+n+'_lock_acquire(void);void esp_crypto_'+n+'_lock_release(void);\n' for n in names))
results=[]
for name,source,patch in [('baseline',here/'sdk-original.c',0),('patched',sketch/'Tab5CryptoLockCompat.c',1)]:
    (build/'LockSource.inc').write_bytes(source.read_bytes())
    args=['cl','/nologo','/utf-8','/EHsc','/std:c++17','/MT','/D_CRT_SECURE_NO_WARNINGS','/DEXPECT_PATCHED='+str(patch),'/I'+str(stubs),'/I'+str(build),str(here/'main.cpp'),'/Fe:'+str(build/(name+'.exe'))]
    compilation=subprocess.run(args,cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace')
    if compilation.returncode:print(compilation.stdout+compilation.stderr);raise SystemExit(compilation.returncode)
    run=subprocess.run([str(build/(name+'.exe'))],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=30)
    print(run.stdout+run.stderr,flush=True);results.append(dict(name=name,exitCode=run.returncode,output=run.stdout+run.stderr))
# The compatibility translation unit must leave SDK symbols to other targets
# and versions. Defining representative SDK symbols here catches accidental
# replacement at compile time, without merely testing a duplicated predicate.
(build/'LockSource.inc').write_bytes((sketch/'Tab5CryptoLockCompat.c').read_bytes())
(build/'guard.cpp').write_text('#include "LockSource.inc"\nstatic int used=0;\nvoid esp_crypto_ecdsa_lock_acquire(){++used;}\nvoid esp_crypto_ecdsa_lock_release(){++used;}\nint main(){esp_crypto_ecdsa_lock_acquire();esp_crypto_ecdsa_lock_release();return used==2?0:1;}\n')
for name,define in [('other-target','TEST_OTHER_TARGET'),('future-sdk','TEST_FUTURE_SDK')]:
    compilation=subprocess.run(['cl','/nologo','/utf-8','/EHsc','/std:c++17','/MT','/D'+define,'/I'+str(stubs),'/I'+str(build),str(build/'guard.cpp'),'/Fe:'+str(build/(name+'.exe'))],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace')
    if compilation.returncode:print(compilation.stdout+compilation.stderr);raise SystemExit(compilation.returncode)
    run=subprocess.run([str(build/(name+'.exe'))],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=30)
    print(name,'PASS' if run.returncode==0 else 'FAIL',flush=True);results.append(dict(name=name,exitCode=run.returncode,output='SDK implementation retained'))
report=dict(version='0.8.28',checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=all(x['exitCode']==0 for x in results),results=results,physicalDeviceTested=False,method='Real complete SDK lock module and production compatibility source; OS locks use std::mutex and target capability boundaries are simulated. 33 scheduled exclusion trials, revision-independent MPI ordering, 8000 mixed concurrent lock acquisitions. Baseline intentionally demonstrates omitted exclusion, not a hardware reset. Other-target/future-version guards compile without replacing SDK symbols.',upstreamCommit='735507283d',upstreamFix='7fc2aaf',sources={'Tab5CryptoLockCompat.c':hashlib.sha256((sketch/'Tab5CryptoLockCompat.c').read_bytes()).hexdigest()},tests={str(p.relative_to(release)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__),here/'main.cpp',here/'sdk-original.c']})
(release/'tools/verification/crypto-lock-0828.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
