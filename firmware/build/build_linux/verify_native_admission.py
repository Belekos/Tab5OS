"""Verify bounded admission is enabled only for the checked allocator binary."""
from pathlib import Path
import hashlib,json,re,subprocess
from verify_heap_probe import probe_sdk

def verify(env,elf,objdump):
    env=Path(env);qa=env/'qa'
    report=json.loads((qa/'heap-probe-sdk.json').read_text())
    current=probe_sdk(Path(report['sdk']))
    if {k:v for k,v in report.items() if k!='verifierSha256'} != current:
        raise RuntimeError('Allocator SDK changed since compilation')
    verifier=Path(__file__).with_name('verify_heap_probe.py')
    if report['verifierSha256']!=hashlib.sha256(verifier.read_bytes()).hexdigest():
        raise RuntimeError('Allocator identity checker changed since compilation')
    args=json.loads((qa/'compile-args.json').read_text())
    for language in ('c','cpp'):
        prefix='compiler.'+language+'.extra_flags='
        choices=[x for x in args['command'] if x.startswith(prefix)]
        if len(choices)!=1 or report['compilerDefine'] not in choices[0].split():
            raise RuntimeError('Allocator identity macro missing from '+language+' flags')
    disassembly=subprocess.check_output([str(objdump),'-d','--disassemble=tab5HeapCanAllocate',str(elf)],text=True)
    (qa/'heap-probe-linked-disassembly.log').write_text(disassembly)
    calls=re.findall(r'\b(?:jal|jalr|j)\b[^\n]*<([^>]+)>',disassembly)
    required={'multi_heap_internal_lock','multi_heap_internal_unlock','esp_heap_adjust_alignment_to_hw'}
    known=report['known']
    if '<tab5HeapCanAllocate>:' not in disassembly:raise RuntimeError('Admission adapter not linked')
    if known and not required.issubset(calls):raise RuntimeError('Checked SDK did not link bounded native admission: '+repr(calls))
    if not known and required.intersection(calls):raise RuntimeError('Unknown SDK linked private allocator access')
    if any(name.startswith(('tlsf_walk_pool','heap_caps_get_largest_free_block','multi_heap_get_info')) for name in calls):
        raise RuntimeError('Bounded native admission unexpectedly walks the heap')
    return {'passed':True,'mode':report['mode'],'knownSdk':known,'nativeCallTargets':calls,
            'sourceSdkHashes':report['sha256'],'physicalDeviceTested':False,
            'scope':'Final linked bounded-admission entry and SDK hash/compile flags. No hardware duration claim.'}
