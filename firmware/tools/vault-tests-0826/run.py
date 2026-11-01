from pathlib import Path
import os,sys,subprocess,shutil,json,hashlib,time
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.26_Ecriture';work=release.parent
build=Path('C:/workspace/tab5-vault-0826');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
names=['Tab5KeyStore.cpp','Tab5KeyStore.h','Tab5Vault.cpp','Tab5Vault.h','Tab5VaultCore.cpp','Tab5VaultCore.h','Tab5Secrets.cpp','Tab5Secrets.h','Tab5Environment.cpp','Tab5Environment.h']
snapshot={n:hashlib.sha256((sketch/n).read_bytes()).hexdigest() for n in names}
for n in names:shutil.copy2(sketch/n,build/n)
for n in ['Platform.h','test.cpp']:shutil.copy2(here/n,build/n)
for n in ['Arduino.h','Preferences.h','FS.h','SD.h','Tab5Task.h','Tab5Storage.h','Tab5VolumeFS.h','Tab5FilePolicy.h','Tab5CryptoRandom.h','freertos/FreeRTOS.h','freertos/task.h','freertos/semphr.h']:
    p=build/n;p.parent.mkdir(exist_ok=True);p.write_text('#include "Platform.h"\n',encoding='utf-8')
mbed=work/'mbedtls-idf554-0822';objects=Path('C:/workspace/tab5-ssh-0822-host/mbedtls-3.6.5-581ca82878fc')
cmd=['cl','/nologo','/MDd','/D_DEBUG','/W3','/utf-8','/EHsc','/std:c++17','/D_CRT_SECURE_NO_WARNINGS','/I'+str(build),'/I'+str(mbed/'include'),str(build/'test.cpp')]+[str(build/n) for n in names if n.endswith('.cpp')]+[str(p) for p in objects.glob('*.obj')]+['/Fe:'+str(build/'test.exe'),'/link','bcrypt.lib','ws2_32.lib','crypt32.lib','advapi32.lib']
r=subprocess.run(cmd,cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace');(build/'build.log').write_text(r.stdout+r.stderr,encoding='utf-8');print(r.stdout+r.stderr,flush=True);r.check_returncode()
start=time.time();r=subprocess.run([str(build/'test.exe')],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=180);print(r.stdout+r.stderr,flush=True);r.check_returncode()
assert snapshot=={n:hashlib.sha256((sketch/n).read_bytes()).hexdigest() for n in names},'Sources changed during tests'
report=json.loads(r.stdout);report.update(version='0.8.26',passed=True,physicalDeviceTested=False,elapsedSeconds=time.time()-start,sources=snapshot,testSources={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in here.glob('*') if p.is_file()},method='Entire production KeyStore, Vault, VaultCore and Secrets linked with actual mbedTLS 3.6.5 native objects. In-memory FS/NVS, deterministic entropy and task queue substitute the hardware boundaries. Target storage/latency not measured.',cryptoVersion='3.6.5',cryptoObjectSha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in objects.glob('*.obj')})
(release/'tools/verification/vault-0826.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
