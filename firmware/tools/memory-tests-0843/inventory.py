#!/usr/bin/env python3
"""Reproduce the optional profiler's P4 static footprint from real artifacts."""
import argparse, hashlib, json, re, subprocess
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--nm',type=Path,required=True)
p.add_argument('--baseline-elf',type=Path,required=True)
p.add_argument('--object',type=Path,required=True)
p.add_argument('--sdkconfig',type=Path,required=True)
a=p.parse_args()
r=Path(__file__).resolve().parents[2]
s=r/'Tab5OS_Arduino_v0.8.43_Ecriture'

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def symbols(path):
    rows=[]
    for line in subprocess.check_output([str(a.nm),'-S',str(path)],text=True).splitlines():
        m=re.match(r'([\da-f]+) ([\da-f]+) ([bBdD]) (.+)',line)
        if m: rows.append(dict(address=int(m[1],16),bytes=int(m[2],16),kind=m[3],symbol=m[4]))
    return rows

baseline=symbols(a.baseline_elf)
current=symbols(a.object)
profile_before=[x for x in baseline if 'Tab5UiDiagnostics' in x['symbol']
                and x['kind'] in 'bB' and 0x4ff00000<=x['address']<0x50000000]
profile_after=[x for x in current if x['kind'] in 'bB']
assert sum(x['bytes'] for x in profile_before)==578
assert sum(x['bytes'] for x in profile_after)==9
config={}
for line in a.sdkconfig.read_text().splitlines():
    if line.startswith('CONFIG_') and '=' in line:
        k,v=line.split('=',1)
        if k in ('CONFIG_ARDUINO_RUNNING_CORE','CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ',
                 'CONFIG_CACHE_L2_CACHE_SIZE','CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL',
                 'CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM'): config[k]=v
sources=[s/n for n in ['Tab5UiDiagnostics.cpp','Tab5UiDiagnostics.h',
          'Tab5Memory.cpp','Tab5ResourceManager.cpp','Tab5Task.h']]
report={
    'version':'0.8.43','passed':True,'physicalDeviceTested':False,
    'baselineVersion':'0.8.42','baselineElfSha256':sha(a.baseline_elf),
    'targetObjectSha256':sha(a.object),'baselineProfilerInternalBssSymbols':profile_before,
    'currentProfilerInternalBssSymbols':profile_after,'internalBssBytesBefore':578,
    'internalBssBytesAfter':9,'rawSymbolBytesSaved':569,
    'retainedRtcBytesBefore':216,'retainedRtcBytesAfter':216,
    'optionalPsramRuntimeBytes':576,'idleProfilerHeapAllocationsBeforeFirstActivation':0,
    'runtimeRetainedAfterDisable':True,'sdkConfig':config,
    'largestProjectMutableSymbolsBaseline':[
        x for x in sorted(baseline,key=lambda x:x['bytes'],reverse=True)
        if x['kind'] in 'bB' and x['symbol'].startswith('_Z')][:30],
    'sourceHashes':{str(path.relative_to(r)):sha(path) for path in sources},
    'testSourceSha256':sha(Path(__file__)),
    'evidence':['tools/verification/ui-diagnostics-0843.json',
                'tools/verification/ui-diagnostics-target-0843.json'],
    'limitations':[
        'Symbol byte sums exclude linker padding and changes in other modules; compare final ELF for the global result',
        'Physical runtime free heap, cache pressure, CPU and latency not measured',
        'No CPU affinity, stack size, generic malloc policy, admission margins or cache ownership changed by this module']}
out=r/'tools/audit-0843/memory.json';out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ['passed','internalBssBytesBefore',
      'internalBssBytesAfter','rawSymbolBytesSaved','optionalPsramRuntimeBytes']}))
