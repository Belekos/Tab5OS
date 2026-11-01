#!/usr/bin/env python3
"""Verify cache-safe DSI/DW-GDMA code, actual allocation flags and ISR references in the linked ELF.

This is deliberately pinned to the reviewed IDF 5.5.4/GCC14 implementation;
unknown compiler forms fail closed rather than inferring safety from IRAM alone.
"""
from pathlib import Path
import argparse, hashlib, json, re, struct, subprocess

def sections(elf):
    data=Path(elf).read_bytes()
    if data[:6]!=b'\x7fELF\x01\x01':raise ValueError('Expected little-endian ELF32')
    shoff=struct.unpack_from('<I',data,32)[0];size,num,strings=struct.unpack_from('<HHH',data,46)
    rows=[struct.unpack_from('<10I',data,shoff+i*size) for i in range(num)]
    names=data[rows[strings][4]:rows[strings][4]+rows[strings][5]]
    return [{'name':names[r[0]:].split(b'\0',1)[0].decode(),'addr':r[3],'size':r[5],'type':r[1]} for r in rows]

def verify(elf,objdump):
    elf=Path(elf);objdump=Path(objdump); sec=sections(elf)
    def execute(*args):return subprocess.check_output([str(objdump),*args,str(elf)],text=True)
    table=execute('-t');cache={};checks=[]
    def check(name,passed,details):checks.append({'name':name,'passed':bool(passed),'details':details})
    def dis(name):
        if name not in cache:cache[name]=execute('-d','--disassemble='+name)
        return cache[name]
    def syms(name):return [r for r in table.splitlines() if len(r.split())>=5 and r.split()[-1]==name]
    def section(addr):
        return next((r['name'] for r in sec if r['addr']<=addr<r['addr']+r['size'] and r['addr']),None)
    def instructions(name):
        return [(m[1],m[2],m[3]) for m in re.finditer(r'^[ \t]*([0-9a-f]+):[ \t]+[0-9a-f]+[ \t]+([a-z][a-z0-9.]*)[ \t]*(.*)$',dis(name),re.M)]
    def call_windows(name,target,before=16):
        ins=instructions(name)
        return [ins[max(0,i-before):i] for i,(_,op,arg) in enumerate(ins) if op in ('jal','jalr','j') and '<'+target+'>' in arg]
    def register(window,reg):
        value=None
        for _,op,arg in window:
            args=arg.split('#',1)[0].strip().split(',')
            if not args or args[0]!=reg:continue
            try:
                if op=='li':value=int(args[1],0)
                elif op=='lui':value=int(args[1],0)<<12
                elif op in ('addi','ori','andi') and args[1]==reg and value is not None:
                    k=int(args[2],0);value=value+k if op=='addi' else value|k if op=='ori' else value&k
                elif op in ('sw','sb','sh','beq','bne','beqz','bnez'):pass
                else:value=None
            except (IndexError,ValueError):value=None
        return value
    iram=['mipi_dsi_bridge_isr_handler','mipi_dsi_dma_trans_done_cb','dw_gdma_channel_default_isr','dw_gdma_channel_enable_ctrl','dw_gdma_channel_use_link_list','dw_gdma_link_list_get_item','dw_gdma_lli_set_block_markers','tab5_display_underrun_isr','esp_timer_get_time','_ZN12_GLOBAL__N_113observeBridgeEPv']
    refresh=[r.split()[-1] for r in table.splitlines() if 'onRefresh' in r and '.iram0.text' in r.split()]
    check('one refresh callback',len(refresh)==1,refresh)
    iram+=refresh
    for name in iram:
        rows=syms(name);check('IRAM '+name,len(rows)==1 and '.iram0.text' in rows[0].split(),rows)
    dpi=call_windows('esp_lcd_new_panel_dpi','__wrap_esp_intr_alloc')
    check('DSI IRQ actual IRAM flags',len(dpi)==1 and register(dpi[0],'a1')==1038,{'a1':[register(w,'a1') for w in dpi],'expected':1038})
    check('DSI IRQ wrapper used',bool(dpi) and not call_windows('esp_lcd_new_panel_dpi','esp_intr_alloc'),len(dpi))
    dw=dis('dw_gdma_channel_register_event_callbacks')
    # Native priority is either LOWMED or one selected bit. Both paths must
    # retain SHARED|IRAM; checking just the default immediate is insufficient.
    check('DW IRQ default IRAM flags',bool(re.search(r'\bli\s+a1,1294\b',dw)),1294)
    check('DW IRQ chosen priority retains IRAM',bool(re.search(r'\bori\s+a1,a1,1280\b',dw)),1280)
    check('DW IRQ uses native interrupt allocator',len(call_windows('dw_gdma_channel_register_event_callbacks','esp_intr_alloc_intrstatus'))==1,'esp_intr_alloc_intrstatus')
    for name,sizes in [('esp_lcd_new_dsi_bus',[28]),('esp_lcd_new_panel_dpi',[136]),('dw_gdma_new_channel',[44,36]),('dw_gdma_new_link_list',[12])]:
        calls=call_windows(name,'heap_caps_calloc'); values=[{'size':register(w,'a1'),'caps':register(w,'a2')} for w in calls]
        for size in sizes:check('internal metadata '+name+':'+str(size),any(v['size']==size and v['caps']==2052 for v in values),values)
    links=call_windows('dw_gdma_new_link_list','heap_caps_aligned_calloc')
    check('DMA link items internal',len(links)==1 and register(links[0],'a3')==2056,[register(w,'a3') for w in links])
    # References printed by objdump are resolved final addresses, including
    # format strings, TAG, __FUNCTION__ and anonymous atomic state.
    guards={'dw_gdma_channel_enable_ctrl','dw_gdma_channel_use_link_list','dw_gdma_link_list_get_item','dw_gdma_lli_set_block_markers'}
    for name in iram:
        refs=[];targets=[]
        for _,op,arg in instructions(name):
            if op in ('jal','jalr','j'):
                m=re.search(r'(?:#\s*)?([0-9a-f]{8})\s+<([^>]+)>',arg)
                if m:
                    address=int(m[1],16);target=m[2].split('+',1)[0];targets.append({'name':target,'section':section(address)})
            elif '#' in arg:
                m=re.search(r'#\s*([0-9a-f]+)\s+<([^>]+)>',arg)
                if m:refs.append({'address':m[1],'name':m[2],'section':section(int(m[1],16))})
        check('no Flash data '+name,all(not str(r['section']).startswith('.flash') for r in refs),refs)
        def allowed_rom(target):
            expected=0x4fc00024 if target['name']=='esp_rom_printf' and name in guards else 0x4fc00234 if target['name']=='systimer_hal_get_counter_value' and name=='esp_timer_get_time' else None
            rows=syms(target['name'])
            return expected is not None and len(rows)==1 and '*ABS*' in rows[0].split() and int(rows[0].split()[0],16)==expected
        bad=[t for t in targets if t['section'] not in ('.iram0.text','.tcm.text') and not allowed_rom(t)]
        check('cache-safe direct calls '+name,not bad,targets)
        if name=='tab5_display_underrun_isr':
            check('underrun hook has no IO',bool(targets) and all(t['name'] in (name,'esp_timer_get_time') for t in targets),targets)
        if name=='mipi_dsi_bridge_isr_handler':
            check('strong native underrun hook called',any(t['name']=='tab5_display_underrun_isr' for t in targets),targets)
    hook=syms('tab5_display_underrun_isr')
    check('underrun hook strong',len(hook)==1 and 'g' in hook[0].split() and 'w' not in hook[0].split(),hook)
    return {'passed':all(c['passed'] for c in checks),'checks':checks,'failures':[c['name'] for c in checks if not c['passed']],'elfSha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'physicalDeviceTested':False,'evidence':cache,'limits':['Static linked-ELF checks of the pinned implementation, not physical cache-off or underrun timing measurements.','Indirect SDK event callbacks rely on native registration IRAM/context validation; no global memory isolation asserted.']}

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--elf',type=Path,required=True);p.add_argument('--objdump',type=Path,required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
    report=verify(a.elf,a.objdump);a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'passed':report['passed'],'checks':len(report['checks']),'failures':report['failures'],'report':str(a.report)}));raise SystemExit(0 if report['passed'] else 1)
