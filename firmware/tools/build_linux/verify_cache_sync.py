"""Bind the cache-sync adapter to actual final ELF calls, not only link flags."""
import json,re,subprocess
from pathlib import Path

def verify(elf, objdump, output):
    elf=Path(elf); output=Path(output)
    table=subprocess.check_output([str(objdump),'-t',str(elf)],text=True)
    symbols=('__wrap_esp_cache_msync','esp_cache_msync','jpeg_decoder_process')
    rows={name:[line for line in table.splitlines() if line.split() and line.split()[-1]==name] for name in symbols}
    failures=[]
    for name,found in rows.items():
        if len(found)!=1:failures.append('Expected one symbol: '+name)
    for name in symbols[:2]:
        if len(rows[name])==1 and '.iram0.text' not in rows[name][0].split():failures.append('Cache path outside IRAM: '+name)
    disassembly={}
    for name in symbols:
        disassembly[name]=subprocess.check_output([str(objdump),'-d','--disassemble='+name,str(elf)],text=True)
    call=lambda text,name:bool(re.search(r'\b(?:jal|jalr|j)\b[^\n]*<'+re.escape(name)+r'>',text))
    if not call(disassembly['__wrap_esp_cache_msync'],'esp_cache_msync'):failures.append('Wrapper does not reach native SDK implementation')
    if not call(disassembly['jpeg_decoder_process'],'__wrap_esp_cache_msync'):failures.append('JPEG DMA preparation bypasses wrapper')
    if call(disassembly['jpeg_decoder_process'],'esp_cache_msync'):failures.append('JPEG DMA preparation still calls original cache entry directly')
    output.write_text('\n'.join(disassembly.values()))
    return {'passed':not failures,'failures':failures,'symbols':rows,
            'jpegCallsWrapper':call(disassembly['jpeg_decoder_process'],'__wrap_esp_cache_msync'),
            'wrapperCallsNative':call(disassembly['__wrap_esp_cache_msync'],'esp_cache_msync'),
            'scope':'Final linked instruction targets and IRAM sections only; no device latency or display result.',
            'physicalDeviceTested':False}
