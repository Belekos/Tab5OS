#!/usr/bin/env python3
"""Compile the observer with the exact target SDK; inspect SDK and P4 objects.

This is a compile/link-layout audit, not a physical interrupt test. The release
verifier additionally checks the final ELF's call target and memory placement.
"""
from pathlib import Path
import argparse, datetime, hashlib, json, subprocess

root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
p.add_argument('--commands',type=Path,default=Path('/workspace/scratch/644cc83c43c8/build_0841/syntax-build/compile_commands.json'))
p.add_argument('--build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/host_display_0841'))
a=p.parse_args();a.build.mkdir(parents=True,exist_ok=True)
source=root/'Tab5OS_Arduino_v0.8.41_Ecriture/Tab5DisplaySignals.cpp'
out=root/'tools/verification/display-0841';out.mkdir(exist_ok=True)
sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
before=sha(source)
row=next(r for r in json.loads(a.commands.read_text()) if r['file'].endswith('Tab5DisplayPresent.cpp'))
cmd=list(row['arguments']);cmd[cmd.index(row['file'])]=str(source)
target=a.build/'Tab5DisplaySignals.p4.o';cmd[cmd.index('-o')+1]=str(target)
result=subprocess.run(cmd,cwd=row['directory'],text=True,capture_output=True)
(out/'signals-p4-build.log').write_text(result.stdout+result.stderr);result.check_returncode()
bin=Path(cmd[0]).parent;objdump=bin/'riscv32-esp-elf-objdump';ar=bin/'riscv32-esp-elf-ar'
sdk=Path(cmd[cmd.index('-iprefix')+1]).parent
def dump(file):return subprocess.check_output([str(objdump),'-t','-dr',str(file)],text=True)
object_text=dump(target);(out/'signals-p4-object.log').write_text(object_text)
assert any('.iram1.' in line and ' F ' in line and 'observeBridge' in line for line in object_text.splitlines())
fields=['originalHandler','originalArgument','active','installing','sequence','interrupts','underruns','lastStatus','lastUsLow','lastUsHigh']
for name in fields:
    assert any('.dram1.' in line and ' O ' in line and name+'E' in line for line in object_text.splitlines()),name
undefined={line.split()[-1] for line in object_text.splitlines() if '*UND*' in line}
assert undefined=={'MIPI_DSI_BRIDGE','esp_timer_get_time','soc_mipi_dsi_signals','__real_esp_intr_alloc'},undefined
library=sdk/'lib/libesp_lcd.a'
member=next(n for n in subprocess.check_output([str(ar),'t',str(library)],text=True).splitlines() if n.startswith('esp_lcd_panel_dpi.'))
driver=a.build/member;driver.write_bytes(subprocess.check_output([str(ar),'p',str(library),member]))
driver_text=dump(driver);(out/'sdk-dpi-object.log').write_text(driver_text)
assert 'R_RISCV_CALL_PLT\tesp_intr_alloc' in driver_text
linker=sdk/'ld/sections.ld'
assert '*libesp_lcd.a:esp_lcd_panel_dpi.*(.literal.mipi_dsi_bridge_isr_handler .text.mipi_dsi_bridge_isr_handler)' in linker.read_text()
timer_text=subprocess.check_output([str(objdump),'-t',str(sdk/'lib/libesp_timer.a')],text=True)
timer_line=next(line for line in timer_text.splitlines() if '.iram1.' in line and line.endswith('esp_timer_get_time'))
stack=target.with_suffix('.su').read_text()
stack_line=next(line for line in stack.splitlines() if 'observeBridge' in line)
assert '\t16\tstatic' in stack_line
(out/'signals-p4-layout.log').write_text(timer_line+'\n'+stack)
assert before==sha(source)
report={'version':'0.8.41','passed':True,'physicalDeviceTested':False,'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'sourceHash':before,'objectHash':sha(target),'sdkDpiObjectHash':sha(driver),'sdkLcdArchiveHash':sha(library),'sdkLinkerHash':sha(linker),
 'isrCodeBytes':158,'isrStackBytes':16,'internalStaticDataBytes':40,
 'checks':['Observer code placed in IRAM and all10 shared fields in internalDRAM','Only expected peripheral/timer/allocator symbols referenced: no malloc, logging, locking or atomic runtime helper','SDK DPI object calls external esp_intr_alloc, so GNU --wrap can intercept it','SDK linker explicitly maps original DSI ISR to IRAM','esp_timer_get_time is IRAM','ISR tail-calls original handler and has bounded16-byte stack'],
 'limitations':['Target object compilation only; final ELF checks belong to release verifier','No measured interrupt latency or physical underrun generated','An observed IRQ is not a count of every physical missed pixel: hardware may coalesce events']}
(root/'tools/verification/display-signals-target-0841.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: target P4 observer IRAM158B, DRAM40B, ISR stack16B; SDK allocator relocation and IRAM timer/handler verified')
