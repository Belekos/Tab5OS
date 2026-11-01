#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib, importlib.util, json, os, subprocess, tempfile

here=Path(__file__).resolve().parent
release=here.parents[1]
sketch=release/'Tab5OS_Arduino_v0.8.49_Ecriture'
upstream=release/'tools/audit-0849/heap-walk/upstream'
parser=argparse.ArgumentParser()
parser.add_argument('--db',type=Path)
args=parser.parse_args()
report={}
spec=importlib.util.spec_from_file_location('verify_heap_probe',release/'tools/build_linux/verify_heap_probe.py')
verifier=importlib.util.module_from_spec(spec);spec.loader.exec_module(verifier)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
with tempfile.TemporaryDirectory(prefix='tab5-admission-0849-') as temporary:
    work=Path(temporary)
    flags=['-g','-O1','-fsanitize=address,undefined','-fno-sanitize=alignment','-fno-omit-frame-pointer',
           '-I',str(sketch),'-I',str(upstream),'-I',str(here/'stubs')]
    objects=[]
    for source in [upstream/'multi_heap.c',upstream/'tlsf.c',sketch/'Tab5HeapProbe.c']:
        obj=work/(source.stem+'.o')
        subprocess.run(['gcc','-std=gnu11',*flags,'-c',str(source),'-o',str(obj)],check=True)
        objects.append(str(obj))
    exe=work/'integration'
    subprocess.run(['g++','-std=c++17','-pthread',*flags,str(here/'fixture.cpp'),
        str(sketch/'Tab5ResourceManager.cpp'),*objects,'-o',str(exe)],check=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    run=subprocess.run([str(exe)],text=True,capture_output=True,env=env)
    print(run.stdout,run.stderr,end='');run.check_returncode()
    report.update(json.loads(run.stdout))
    if args.db:
        db=json.loads(args.db.read_text())
        target={}
        c_row=next(r for r in db if Path(r['file']).name=='Tab5ImageCore.c')
        sdk=Path(next(p[1:] for p in c_row['arguments'] if p.startswith('@') and p.endswith('/flags/c_flags'))).parent.parent
        pin=verifier.probe_sdk(sdk);assert pin['known'],pin
        assert not verifier.probe_sdk(work/'missing-sdk')['known']
        report['sdkPin']=pin
        for name,template in [('Tab5HeapProbe.c','Tab5ImageCore.c'),('Tab5ResourceManager.cpp','Tab5ResourceManager.cpp'),('Tab5Memory.cpp','Tab5Memory.cpp')]:
            row=next(r for r in db if Path(r['file']).name==template)
            command=list(row['arguments']);command[command.index(row['file'])]=str(sketch/name)
            command[command.index('-o')+1]=str(work/(name+'.target.o'))
            command=[p for p in command if p!='-w' and not p.startswith('-DTAB5_HEAP_PROBE_PINNED=')]
            command.append(pin['compilerDefine'])
            result=subprocess.run(command,cwd=row['directory'],text=True,capture_output=True)
            target[name]={'returncode':result.returncode,'diagnostics':result.stdout+result.stderr}
            if result.returncode:print(result.stdout,result.stderr)
            result.check_returncode()
            if name=='Tab5HeapProbe.c':
                objdump=Path(command[0]).with_name('riscv32-esp-elf-objdump')
                native=subprocess.run([str(objdump),'-dr','--disassemble=tab5HeapCanAllocate',command[command.index('-o')+1]],text=True,capture_output=True,check=True).stdout
                assert 'multi_heap_internal_lock' in native and 'multi_heap_internal_unlock' in native
                target[name]['nativeDisassemblySha256']=hashlib.sha256(native.encode()).hexdigest()
                command[-1]='-DTAB5_HEAP_PROBE_PINNED=0'
                fallback=work/'probe-fallback.o';command[command.index('-o')+1]=str(fallback)
                subprocess.run(command,cwd=row['directory'],check=True)
                safe=subprocess.run([str(objdump),'-dr','--disassemble=tab5HeapCanAllocate',str(fallback)],text=True,capture_output=True,check=True).stdout
                assert 'multi_heap_internal_lock' not in safe
                target[name]['fallbackDisassemblySha256']=hashlib.sha256(safe.encode()).hexdigest()
        report['targetCompile']=target;report['compileDatabaseSha256']=sha(args.db)
report.update({'physicalDeviceTested':False,'sanitizers':['ASan','UBSan excluding alignment: upstream ESP TLSF uses 4-byte block alignment in an LP64 host ABI','LSan disabled for container /proc restrictions'],
 'limits':['Allocator ABI is host LP64; P4 adapter separately target-compiled. No physical latency claim.',
           'Native TLSF allocator and byte-for-byte nonmutation tested; peripheral and task creation not exercised.',
           'Native fit is a snapshot. The actual allocation remains authoritative under concurrency.'],
 'sourceHashes':{str(p.relative_to(release)):sha(p) for p in [here/'fixture.cpp',Path(__file__),sketch/'Tab5HeapProbe.c',sketch/'Tab5HeapProbe.h',sketch/'Tab5HeapFit.h',sketch/'Tab5Memory.cpp',sketch/'Tab5ResourceManager.cpp',sketch/'Tab5ResourcePolicy.h',upstream/'tlsf.c',upstream/'tlsf_control_functions.h']}})
(here/'results.json').write_text(json.dumps(report,indent=2)+'\n')
