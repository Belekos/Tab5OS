"""Reproduce failed-preload UI retry on immutable .26; verify .27 with actual crypto."""
from pathlib import Path
import os,sys,subprocess,shutil,json,hashlib
here=Path(__file__).resolve().parent;release=here.parent.parent;work=release.parent
build=Path('C:/workspace/tab5-vault-reopen-0827');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
names=['Tab5KeyStore.cpp','Tab5KeyStore.h','Tab5Vault.cpp','Tab5Vault.h','Tab5VaultCore.cpp','Tab5VaultCore.h','Tab5Secrets.cpp','Tab5Secrets.h','Tab5Environment.cpp','Tab5Environment.h']
rows=[];mbed=work/'mbedtls-idf554-0822';referenceObjects=Path('C:/workspace/tab5-ssh-0822-host/mbedtls-3.6.5-581ca82878fc');objects=build/'crypto-mt-asan';objects.mkdir(exist_ok=True)
cryptoSources=[mbed/'library'/(p.stem+'.c') for p in referenceObjects.glob('*.obj')]
if not (objects/'complete.json').exists():
    cmd=['cl','/nologo','/MT','/MP4','/std:c11','/fsanitize=address','/Z7','/D_CRT_SECURE_NO_WARNINGS','/I'+str(mbed/'include'),'/I'+str(mbed/'library'),'/c',*[str(p) for p in cryptoSources]]
    c=subprocess.run(cmd,cwd=objects,capture_output=True,text=True,encoding='utf-8',errors='replace');(objects/'build.log').write_text(c.stdout+c.stderr,encoding='utf-8');print(c.stdout+c.stderr,flush=True);c.check_returncode()
    (objects/'complete.json').write_text(json.dumps({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in cryptoSources}),encoding='utf-8')
for tag in ['0826','0827']:
    version='0.8.'+tag[-2:];sourceRelease=work/('Tab5OS_Ecriture_v'+version);sketch=sourceRelease/('Tab5OS_Arduino_v'+version+'_Ecriture');out=build/tag;out.mkdir(exist_ok=True)
    snapshot={n:hashlib.sha256((sketch/n).read_bytes()).hexdigest() for n in names}
    for n in names:shutil.copy2(sketch/n,out/n)
    for n in ['Platform.h','reopen_regression.cpp']:shutil.copy2(here/n,out/n)
    for n in ['Arduino.h','Preferences.h','FS.h','SD.h','Tab5Task.h','Tab5Diagnostics.h','Tab5Storage.h','Tab5VolumeFS.h','Tab5FilePolicy.h','Tab5CryptoRandom.h','freertos/FreeRTOS.h','freertos/task.h','freertos/semphr.h']:
        p=out/n;p.parent.mkdir(exist_ok=True);p.write_text('#include "Platform.h"\n',encoding='utf-8')
    cmd=['cl','/nologo','/MT','/W3','/utf-8','/EHsc','/std:c++17','/fsanitize=address','/Z7','/D_CRT_SECURE_NO_WARNINGS','/I'+str(out),'/I'+str(mbed/'include'),str(out/'reopen_regression.cpp')]+[str(out/n) for n in names if n.endswith('.cpp')]+[str(p) for p in objects.glob('*.obj')]+['/Fe:'+str(out/'test.exe'),'/link','/INCREMENTAL:NO','bcrypt.lib','ws2_32.lib','crypt32.lib','advapi32.lib']
    c=subprocess.run(cmd,cwd=out,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/'build.log').write_text(c.stdout+c.stderr,encoding='utf-8');print(c.stdout+c.stderr,flush=True);c.check_returncode()
    run=subprocess.run([str(out/'test.exe')],cwd=out,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=120);print(tag,run.returncode,run.stdout,run.stderr,flush=True)
    assert snapshot=={n:hashlib.sha256((sketch/n).read_bytes()).hexdigest() for n in names},'Sources changed during test'
    row=dict(version=version,exitCode=run.returncode,output=run.stdout+run.stderr,sources=snapshot)
    try:row['observations']=json.loads(run.stdout)
    except ValueError:pass
    rows.append(row)
passed=rows[0]['exitCode']==1 and rows[1]['exitCode']==0 and rows[0].get('observations',{}).get('uiFileOperationsAfterFailedPreload',0)>0
report=dict(version='0.8.27',passed=passed,physicalDeviceTested=False,addressSanitizer=True,rows=rows,testSources={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [here/'Platform.h',here/'reopen_regression.cpp',Path(__file__)]},method='Actual complete KeyStore, Secrets, Environment and crypto units with actual mbedTLS3.6.5; NVS/FS/RTOS mocked. Same damaged-bank fixture on .26 and .27. Reproduces repeated file/crypto on post-unlock UI calls and premature success status, not the physical reset itself.')
(release/'tools/verification/vault-reopen-0827.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if passed else 1)
