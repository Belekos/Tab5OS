#!/usr/bin/env python3
"""Exercise exact production display/cache C++ with explicit Linux hardware doubles."""
from pathlib import Path
import argparse, datetime, hashlib, json, os, re, subprocess
root=Path(__file__).resolve().parents[2]
here=Path(__file__).resolve().parent
sketch=root/'Tab5OS_Arduino_v0.8.33_Ecriture'
p=argparse.ArgumentParser()
p.add_argument('--build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/host_display_0833'))
a=p.parse_args();a.build.mkdir(parents=True,exist_ok=True)
exe=a.build/'test'
cmd=['c++','-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(here/'stubs'),'-I'+str(sketch),str(here/'main.cpp'),str(sketch/'Tab5DisplayPresent.cpp'),str(sketch/'Tab5DisplayCache.cpp'),'-o',str(exe)]
subprocess.run(cmd,check=True);exe.chmod(0o755)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
r=subprocess.run([str(exe)],env=env,text=True,capture_output=True,check=True)
print(r.stdout,end='')
counts={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',r.stdout)}
files=[*sorted(sketch.glob('Tab5Display*')),*sorted(p for p in here.rglob('*') if p.is_file())]
report={
 'version':'0.8.33','passed':True,'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'checks':counts['checks'],'counts':counts,
 'sourceHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},
 'scope':'Exact production Tab5DisplayPresent.cpp and Tab5DisplayCache.cpp under ASan/UBSan; Linux low-address framebuffer memory and explicit SDK/task/IRQ/cache doubles.',
 'productionChange':'None: independent review found no display-driver regression requiring a speculative rewrite.',
 'scenarios':['250 consecutive partial frame publications preserve earlier pixels across both buffers','A single refresh event never releases the previous front; delayed second event drops the whole LVGL frame without writing scanned pixels','Injected publication failure leaves visible bytes intact and requests a paced complete repaint','Injected cache-mirror failure preserves the visible framebuffer and retries only after error backoff','Actual cache-range filtering and cache failure recovery cooperate with the present state machine','30 million simulated milliseconds without new rendering produce no copying, publication, clearing or redraw','Counter wrap and non-owner task calls','No framebuffer allocation/free during normal Begin/End rendering; fixed metadata allocation at initialization'],
 'reviewFindings':['M5GFX 0.2.30 Panel_FrameBufferBase::display only performs cache writeback; publication occurs once through Tab5DisplayPresentEndFrame','Display retry timers are dormant without a prior publication/cache error, unlike the resource-manager periodic purge fixed separately','The two-after-publication refresh fence is deliberately conservative and was not shortened','Full-frame ELF image UI generates unnecessary PSRAM traffic; hardware bandwidth saturation remains a hypothesis until target measurement'],
 'primarySources':['https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/peripherals/lcd/dsi_lcd.html'],
 'limitations':['No ESP32-P4, DSI panel, real ISR scheduling, DMA underrun, memory-bus contention or visible flash measurement','Tests use API doubles and do not execute the ESP-IDF driver','ASan/UBSan enabled; LeakSanitizer disabled because this environment cannot read process task metadata','Passing this suite does not establish that all user-reported flashes are eliminated']}
(root/'tools/verification/display-0833.json').write_text(json.dumps(report,indent=2)+'\n')
