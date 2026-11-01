#!/usr/bin/env python3
"""Exercise exact production display/cache C++ with explicit Linux hardware doubles."""
from pathlib import Path
import argparse, datetime, hashlib, json, os, re, subprocess
root=Path(__file__).resolve().parents[2]
here=Path(__file__).resolve().parent
sketch=root/'Tab5OS_Arduino_v0.8.46_Ecriture'
p=argparse.ArgumentParser()
p.add_argument('--build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/host_display_0846'))
p.add_argument('--lvgl',type=Path,default=Path('/workspace/scratch/644cc83c43c8/build_0842/host-dependencies/lvgl-9.6.0'))
p.add_argument('--lvgl-build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/host_lvgl_0842'))
a=p.parse_args();a.build.mkdir(parents=True,exist_ok=True)
sourceBefore={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(sketch.glob('Tab5Display*'))}
exe=a.build/'test'
cmd=['c++','-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(here/'stubs'),'-I'+str(sketch),str(here/'main.cpp'),str(sketch/'Tab5DisplayPresent.cpp'),str(sketch/'Tab5DisplayCache.cpp'),'-o',str(exe)]
out=root/'tools/verification/display-0846';out.mkdir(exist_ok=True)
build=subprocess.run(cmd,capture_output=True,text=True);(out/'build.log').write_text(build.stdout+build.stderr);build.check_returncode();exe.chmod(0o755)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
r=subprocess.run([str(exe)],env=env,text=True,capture_output=True,check=True)
print(r.stdout,end='');(out/'run.log').write_text(r.stdout+r.stderr)
counts={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',r.stdout)}
activityExe=a.build/'activity'
cmd=['c++','-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build/'lv_conf.h')+'"','-I'+str(here/'stubs'),'-I'+str(sketch),'-I'+str(a.lvgl),str(here/'activity.cpp'),str(sketch/'Tab5DisplayActivity.cpp'),str(a.lvgl_build/'liblvgl.a'),'-pthread','-o',str(activityExe)]
build=subprocess.run(cmd,capture_output=True,text=True);(out/'activity-build.log').write_text(build.stdout+build.stderr);build.check_returncode()
activity=subprocess.run([str(activityExe)],env=env,text=True,capture_output=True);(out/'activity-run.log').write_text(activity.stdout+activity.stderr);activity.check_returncode();print(activity.stdout,end='')
pipelineExe=a.build/'pipeline'
cmd=['c++','-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build/'lv_conf.h')+'"','-I'+str(here/'stubs'),'-I'+str(sketch),'-I'+str(a.lvgl),str(here/'pipeline.cpp'),str(sketch/'Tab5DisplayRefresh.cpp'),str(sketch/'Tab5DisplayPresent.cpp'),str(sketch/'Tab5DisplayCache.cpp'),str(sketch/'Tab5DisplayActivity.cpp'),str(a.lvgl_build/'liblvgl.a'),'-pthread','-o',str(pipelineExe)]
build=subprocess.run(cmd,capture_output=True,text=True);(out/'pipeline-build.log').write_text(build.stdout+build.stderr);build.check_returncode()
pipeline=subprocess.run([str(pipelineExe)],env=env,text=True,capture_output=True);(out/'pipeline-run.log').write_text(pipeline.stdout+pipeline.stderr);print(pipeline.stdout+pipeline.stderr,end='');pipeline.check_returncode()
pipelineCounts={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',pipeline.stdout)}
common=['c++','-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-ffunction-sections','-fdata-sections','-no-pie','-I'+str(here/'stubs'),'-I'+str(sketch)]
sdkObject=a.build/'signals-sdk.o';sdkArchive=a.build/'libsignals-sdk.a'
subprocess.run(common+['-c',str(here/'signals-sdk.cpp'),'-o',str(sdkObject)],check=True,capture_output=True)
subprocess.run(['ar','rcs',str(sdkArchive),str(sdkObject)],check=True,capture_output=True)
signalCounts={}
for mode in ('wrapped','unwrapped'):
    executable=a.build/('signals-'+mode)
    cmd=common+[str(here/'signals.cpp'),str(sketch/'Tab5DisplaySignals.cpp'),str(sdkArchive),'-Wl,--gc-sections','-o',str(executable)]
    if mode=='wrapped':cmd+=['-Wl,--wrap=esp_intr_alloc']
    build=subprocess.run(cmd,capture_output=True,text=True);(out/('signals-'+mode+'-build.log')).write_text(build.stdout+build.stderr);build.check_returncode()
    run=subprocess.run([str(executable),mode],env=env,capture_output=True,text=True);(out/('signals-'+mode+'-run.log')).write_text(run.stdout+run.stderr);run.check_returncode();print(run.stdout,end='')
    signalCounts[mode]={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',run.stdout)}
files=[*sorted(sketch.glob('Tab5Display*')),*sorted(p for p in here.rglob('*') if p.is_file())]
report={
 'version':'0.8.46','passed':True,'physicalDeviceTested':False,'sourceStableDuringTests':sourceBefore=={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(sketch.glob('Tab5Display*'))},'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'checks':counts['checks']+pipelineCounts['checks']+sum(x['checks'] for x in signalCounts.values()),'counts':counts,'pipelineCounts':pipelineCounts,'signalCounts':signalCounts,
 'sourceHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(sketch.glob('Tab5Display*'))},
 'testSourceHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in here.rglob('*') if p.is_file()},
 'evidenceHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in out.iterdir() if p.is_file()},
 'lvglArchiveSha256':hashlib.sha256((a.lvgl_build/'liblvgl.a').read_bytes()).hexdigest(),
 'scope':'Real LVGL 9.6.0 rendering and exact production Tab5DisplayPresent.cpp, Tab5DisplayCache.cpp and Tab5DisplayActivity.cpp under ASan/UBSan. Separate CPU-cache/external-RAM/scanning-framebuffer model, Linux low-address buffers and explicit SDK/task/IRQ/cache doubles. This does not run the complete OS or the ESP-IDF peripheral driver.',
 'productionChange':'Mirror and draw cache ranges are merged for one end-of-frame C2M pass. Generic/unsupported presenter retains immediate writeback. No new framebuffer, changed panel timing, reset, fence or frame suppression.',
 'scenarios':['Real LVGL event attribution: small updates do not overwrite last large-update source/timestamp; partial flushes aggregate into frame sizes; delta counters and millis wraparound remain correct','250 consecutive partial frame publications preserve earlier pixels across both buffers','A single refresh event never releases the previous front; an unguarded attempt returns immediately and safely requests recovery','Injected publication failure leaves visible bytes intact and requests a paced complete repaint','Injected cache-mirror failure preserves the visible framebuffer and retries only after error backoff','Actual cache-range filtering and cache failure recovery cooperate with the present state machine','30 million simulated milliseconds without new rendering produce no copying, publication, clearing or redraw','Counter wrap and non-owner task calls','No framebuffer allocation/free during normal Begin/End rendering; fixed metadata allocation at initialization'],
 'pipelineScenarios':['180000ms of hidden settings RTC label updates: zero new publications or cache writebacks','90 small visible clock updates preserve all unmodified PSRAM pixels','2750ms delay between handlers does not publish an intermediate cleared framebuffer','DMA callbacks paused: LVGL refresh alone pauses; no UI wait or framebuffer write, pending invalidations render after the safe fence','Cache-writeback failure does not publish partially synchronized PSRAM; delayed recovery reconstructs the visible image','128KiB temporary allocation churn produces no unsolicited rendering','All pixel writes and cache writebacks assert that scanning pixels are unchanged'],
 'signalScenarios':['Actual GNU linker --wrap routes only the DSI bridge source; underlying SDK allocator is extracted from a static archive','Ordinary unwrapped link succeeds with Arduino-style --gc-sections and explicitly reports inactive','Allocator failure is propagated unchanged, without reporting active; later successful installation is observed','Every original handler invocation receives the exact original argument and remains the only register acknowledgement','Only UNDERRUN status bit increments the underrun count; VSYNC/unknown/zero status still reaches the original handler','64-bit event timestamp survives the low 32-bit microsecond wrap','1000 diagnostic snapshots cause no hardware reads or ISR allocations'],
 'reviewFindings':['No 90-second display reset or global invalidation timer found. The 90-second Meshtastic deadlines only govern pairing. Energy brightness writes occur only when target brightness changes. Resource service does no periodic heap walk or purge at idle.','M5GFX 0.2.30 Panel_FrameBufferBase::display only performs cache writeback; publication occurs once through Tab5DisplayPresentEndFrame. Its rotation-1 pixel mapping agrees with production dirty-range tracking.','Display retry timers are dormant without a prior publication/cache error; an unobserved error must not be invented to explain idle flashes.','Target SDK is ESP32-P4 pre-revision-3: IDF 5.5.4 invokes on_refresh_done after restarting the DMA descriptor. The two-after-publication fence remains conservative. Revision-3 VSYNC behavior was not physically validated.','M5GFX 0.2.30 and M5Unified 0.2.23 already leave LCD_RST PI4IO0x43 bit4 as input with pull-up; no periodic LCD_RST write was found.','IDF 5.5.4 logs DSI underrun when PSRAM cannot feed the display quickly enough; bus contention is a hardware hypothesis, not reproduced or measured by host tests.','Cache sync is bounded to16KiB calls, but ESP-IDF internally enters critical sections for writeback; claiming that interrupts remain continuously enabled would be inaccurate.'],
 'primarySources':['https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32p4/api-reference/peripherals/lcd/dsi_lcd.html','https://raw.githubusercontent.com/espressif/esp-idf/v5.5.4/components/esp_lcd/dsi/esp_lcd_panel_dpi.c','https://raw.githubusercontent.com/espressif/esp-idf/v5.5.4/components/esp_mm/esp_cache_msync.c','https://downloads.arduino.cc/libraries/github.com/m5stack/M5GFX-0.2.30.zip','https://downloads.arduino.cc/libraries/github.com/m5stack/M5Unified-0.2.23.zip'],
 'limitations':['No ESP32-P4, DSI panel, real ISR scheduling, DMA underrun, memory-bus contention or visible flash measurement','Tests use API doubles and do not execute the ESP-IDF driver','ASan/UBSan enabled; LeakSanitizer disabled because this environment cannot read process task metadata','Passing this suite does not establish that all user-reported flashes are eliminated']}
assert report['sourceStableDuringTests']
(root/'tools/verification/display-0846.json').write_text(json.dumps(report,indent=2)+'\n')
