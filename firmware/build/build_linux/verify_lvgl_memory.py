"""Check the linked LVGL memory hooks use native LVGL TLSF and supplied config."""
from pathlib import Path
import hashlib,re,subprocess

def verify(environment,sketch,elf,objdump):
    env,sketch=Path(environment),Path(sketch);build=env/'build';qa=env/'qa'
    table=subprocess.check_output([str(objdump),'-t',str(elf)],text=True)
    native=('lv_tlsf_create','lv_tlsf_malloc','lv_tlsf_realloc','lv_tlsf_free','lv_tlsf_add_pool','lv_tlsf_remove_pool','lv_tlsf_size')
    hooks=('lv_mem_init','lv_malloc_core','lv_realloc_core','lv_free_core')
    symbols=native+hooks
    rows={name:[line for line in table.splitlines() if line.split() and line.split()[-1]==name] for name in symbols}
    for name,found in rows.items():
        if len(found)!=1:raise RuntimeError('Expected one linked LVGL memory symbol: '+name)
    obj=build/'sketch/Tab5LvglMemory.cpp.o'
    objtable=subprocess.check_output([str(objdump),'-t',str(obj)],text=True)
    for name in hooks:
        if not any(line.split() and line.split()[-1]==name and '*UND*' not in line for line in objtable.splitlines()):
            raise RuntimeError('LVGL hook absent from reviewed adapter object: '+name)
    dep=list(build.rglob('lv_tlsf.c.d'))
    if len(dep)!=1:raise RuntimeError('Expected exactly one compiled native LVGL TLSF unit')
    text=dep[0].read_text().replace('\\\n',' ')
    if str(sketch/'lv_conf.h') not in text:raise RuntimeError('LVGL TLSF did not compile with current sketch configuration')
    (qa/'lvgl-tlsf-dependency.d').write_bytes(dep[0].read_bytes())
    disassembly={name:subprocess.check_output([str(objdump),'-d','--disassemble='+name,str(elf)],text=True) for name in ('lv_mem_init','lv_realloc_core','lv_tlsf_size')}
    def call(text,name):return bool(re.search(r'\b(?:jal|jalr|j)\b[^\n]*<'+re.escape(name)+r'>',text))
    if not call(disassembly['lv_mem_init'],'lv_tlsf_create'):raise RuntimeError('LVGL adapter does not create native TLSF control')
    if not call(disassembly['lv_realloc_core'],'lv_tlsf_realloc'):raise RuntimeError('LVGL realloc hook does not call native LVGL TLSF')
    if call(disassembly['lv_realloc_core'],'heap_caps_realloc'):raise RuntimeError('LVGL realloc hook still uses shared heap realloc')
    (qa/'lvgl-memory-linked-disassembly.log').write_text('\n'.join(disassembly.values()))
    return {'passed':True,'symbols':rows,'configurationSha256':hashlib.sha256((sketch/'lv_conf.h').read_bytes()).hexdigest(),
            'adapterObjectSha256':hashlib.sha256(obj.read_bytes()).hexdigest(),
            'nativeTlsfDependencySha256':hashlib.sha256(dep[0].read_bytes()).hexdigest(),
            'physicalDeviceTested':False,'scope':'Final ELF symbol origin, called allocator and actual native LVGL dependency/configuration. Runtime SRAM/PSRAM use is not inferred.'}
