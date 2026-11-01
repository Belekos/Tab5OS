#!/usr/bin/env python3
"""Exercise exact production display/cache C++ with explicit Linux hardware doubles."""
from pathlib import Path
import argparse, datetime, hashlib, json, os, re, subprocess
root=Path(__file__).resolve().parents[2]
here=Path(__file__).resolve().parent
sketch=root/'Tab5OS_Arduino_v0.8.40_Ecriture'
p=argparse.ArgumentParser()
p.add_argument('--build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/host_display_0840'))
p.add_argument('--lvgl',type=Path,default=Path('/workspace/scratch/644cc83c43c8/build_0840/host-dependencies/lvgl-9.6.0'))
p.add_argument('--lvgl-build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/host_lvgl_0840'))
a=p.parse_args();a.build.mkdir(parents=True,exist_ok=True)
sourceBefore={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(sketch.glob('Tab5Display*'))}
exe=a.build/'test'
cmd=['c++','-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(here/'stubs'),'-I'+str(sketch),str(here/'main.cpp'),str(sketch/'Tab5DisplayPresent.cpp'),str(sketch/'Tab5DisplayCache.cpp'),'-o',str(exe)]
out=root/'tools/verification/display-0840';out.mkdir(exist_ok=True)
build=subprocess.run(cmd,capture_output=True,text=True);(out/'build.log').write_text(build.stdout+build.stderr);build.check_returncode();exe.chmod(0o755)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
r=subprocess.run([str(exe)],env=env,text=True,capture_output=True,check=True)
print(r.stdout,end='');(out/'run.log').write_text(r.stdout+r.stderr)
counts={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',r.stdout)}
activityExe=a.build/'activity'
cmd=['c++','-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build/'lv_conf.h')+'"','-I'+str(here/'stubs'),'-I'+str(sketch),'-I'+str(a.lvgl),str(here/'activity.cpp'),str(sketch/'Tab5DisplayActivity.cpp'),str(a.lvgl_build/'liblvgl.a'),'-pthread','-o',str(activityExe)]
build=subprocess.run(cmd,capture_output=True,text=True);(out/'activity-build.log').write_text(build.stdout+build.stderr);build.check_returncode()
activity=subprocess.run([str(activityExe)],env=env,text=True,capture_output=True);(out/'activity-run.log').write_text(activity.stdout+activity.stderr);activity.check_returncode();print(activity.stdout,end='')
files=[*sorted(sketch.glob('Tab5Display*')),*sorted(p for p in here.rglob('*') if p.is_file())]
report={
 'version':'0.8.40','passed':True,'physicalDeviceTested':False,'sourceStableDuringTests':sourceBefore=={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(sketch.glob('Tab5Display*'))},'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'checks':counts['checks'],'counts':counts,
 'sourceHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(sketch.glob('Tab5Display*'))},
 'testSourceHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in here.rglob('*') if p.is_file()},
 'evidenceHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in out.iterdir() if p.is_file()},
 'lvglArchiveSha256':hashlib.sha256((a.lvgl_build/'liblvgl.a').read_bytes()).hexdigest(),
 'scope':'Actual DisplayActivity under real LVGL 9.6.0 and exact production Tab5DisplayPresent.cpp and Tab5DisplayCache.cpp under ASan/UBSan; Linux low-address framebuffer memory and explicit SDK/task/IRQ/cache doubles.',
 'productionChange':'Passive area/source/timestamp counters added to DisplayActivity. DisplayPresent, DisplayCache and DisplayBandwidth unchanged; no proven driver root cause for the reported rare flashes.',
 'scenarios':['Real LVGL event attribution: small updates do not overwrite last large-update source/timestamp; partial flushes aggregate into frame sizes; delta counters and millis wraparound remain correct','250 consecutive partial frame publications preserve earlier pixels across both buffers','A single refresh event never releases the previous front; delayed second event drops the whole LVGL frame without writing scanned pixels','Injected publication failure leaves visible bytes intact and requests a paced complete repaint','Injected cache-mirror failure preserves the visible framebuffer and retries only after error backoff','Actual cache-range filtering and cache failure recovery cooperate with the present state machine','30 million simulated milliseconds without new rendering produce no copying, publication, clearing or redraw','Counter wrap and non-owner task calls','No framebuffer allocation/free during normal Begin/End rendering; fixed metadata allocation at initialization'],
 'reviewFindings':['No 90-second display reset or global invalidation timer found. The 90-second Meshtastic deadlines only govern pairing. Energy brightness writes occur only when target brightness changes. Resource service remains passive.','M5GFX 0.2.30 Panel_FrameBufferBase::display only performs cache writeback; publication occurs once through Tab5DisplayPresentEndFrame','Display retry timers are dormant without a prior publication/cache error, unlike the resource-manager periodic purge fixed separately','The two-after-publication refresh fence is deliberately conservative and was not shortened','Full-frame ELF image UI generates unnecessary PSRAM traffic; hardware bandwidth saturation remains a hypothesis until target measurement'],
 'primarySources':['https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/peripherals/lcd/dsi_lcd.html'],
 'limitations':['No ESP32-P4, DSI panel, real ISR scheduling, DMA underrun, memory-bus contention or visible flash measurement','Tests use API doubles and do not execute the ESP-IDF driver','ASan/UBSan enabled; LeakSanitizer disabled because this environment cannot read process task metadata','Passing this suite does not establish that all user-reported flashes are eliminated']}
assert report['sourceStableDuringTests']
(root/'tools/verification/display-0840.json').write_text(json.dumps(report,indent=2)+'\n')
