"""Compile real vault modules against pinned Espressif mbedTLS; emulate hardware boundaries only."""
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from test_report_0842 import save_report
import argparse, hashlib, json, os, shutil, subprocess, time, zipfile
p=argparse.ArgumentParser();p.add_argument('--baseline',action='store_true');p.add_argument('--baseline-zip',type=Path);p.add_argument('--mbedtls',type=Path);p.add_argument('--build',type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.42_Ecriture';workspace=release.parent.parent
mbed=(a.mbedtls or workspace/'build_0842/host-dependencies/mbedtls-3.6.5').resolve();build=(a.build or workspace/('build_0842/vault-host-baseline' if a.baseline or a.baseline_zip else 'build_0842/vault-host-current')).resolve();build.mkdir(parents=True,exist_ok=True)
commit=subprocess.check_output(['git','-C',str(mbed),'rev-parse','HEAD'],text=True).strip();assert commit=='ffb280bb63c78bfec1e1ab55040671768c85c923'
names=['Tab5KeyStore.cpp','Tab5KeyStore.h','Tab5Vault.cpp','Tab5Vault.h','Tab5VaultCore.cpp','Tab5VaultCore.h','Tab5Secrets.cpp','Tab5Secrets.h','Tab5Environment.cpp','Tab5Environment.h','Tab5AtomicFile.h']
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
a.baseline=a.baseline or bool(a.baseline_zip)
if a.baseline:
 archive=a.baseline_zip or workspace/'baseline_0842/Tab5OS_0.8.41_Corrections.zip'
 with zipfile.ZipFile(archive) as z:
  for n in names:
   path=next(p for p in z.namelist() if p.endswith('/'+n) and '/tools/' not in p);(build/n).write_bytes(z.read(path))
else:
 for n in names:shutil.copy2(sketch/n,build/n)
snapshot={n:digest(build/n) for n in names}
for n in ['Platform.h','test.cpp','regression.cpp']:shutil.copy2(here/n,build/n)
for n in ['Arduino.h','Preferences.h','FS.h','SD.h','Tab5Task.h','Tab5Diagnostics.h','Tab5Storage.h','Tab5VolumeFS.h','Tab5FilePolicy.h','Tab5CryptoRandom.h','freertos/FreeRTOS.h','freertos/task.h','freertos/semphr.h']:
 dest=build/n;dest.parent.mkdir(exist_ok=True);dest.write_text('#include "Platform.h"\n')
results={};execution='';started=time.monotonic()
for test in (['regression'] if a.baseline else ['regression','test']):
 cmd=['g++','-O2','-g','-std=c++17','-pthread','-I'+str(build),'-I'+str(mbed/'include'),str(build/(test+'.cpp'))]+[str(build/n) for n in names if n.endswith('.cpp')]+[str(mbed/'library/libmbedcrypto.a'),'-o',str(build/test)]
 r=subprocess.run(cmd,capture_output=True,text=True);(build/(test+'-build.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');r.check_returncode()
 r=subprocess.run([str(build/test)]+(['baseline'] if a.baseline else []),capture_output=True,text=True,timeout=180);(build/(test+'-run.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='');r.check_returncode();results[test]=json.loads(r.stdout);execution+=r.stdout+r.stderr
assert a.baseline or snapshot=={n:digest(sketch/n) for n in names},'Sources changed during test'
report=dict(version='0.8.42',baseline=a.baseline,physicalDeviceTested=False,elapsedSeconds=time.monotonic()-started,sources=snapshot,tests=results,cryptoVersion='3.6.5',cryptoCommit=commit,cryptoLibrarySha256=digest(mbed/'library/libmbedcrypto.a'),method='Entire production KeyStore, Vault, VaultCore, Secrets and Environment with real mbedTLS 3.6.5. Files/NVS/tasks are hardware-boundary adapters; state-contention probes use actual host threads/mutexes. No device latency or flash/card durability claim.')
save_report(report,release/'tools/verification'/('vault-0842-baseline.json' if a.baseline else 'vault-0842.json'),here,execution)
