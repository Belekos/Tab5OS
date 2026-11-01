#!/usr/bin/env python3
"""Compare actual .47/.48 presenter/cache and IDF draw/DMA callback under host memory model."""
from pathlib import Path
import argparse, datetime, hashlib, json, os, shutil, subprocess
release=Path(__file__).resolve().parents[2]
sketch=next(release.glob('Tab5OS_Arduino_*_Ecriture'))
workspace=release.parents[1]; tests=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path,default=workspace/'baseline_0847/Tab5OS_Ecriture_v0.8.47/Tab5OS_Arduino_v0.8.47_Ecriture');p.add_argument('--build',type=Path,default=workspace/'build_0848/display');a=p.parse_args();a.build.mkdir(parents=True,exist_ok=True)
proof=release/'tools/verification/display-0848';proof.mkdir(parents=True,exist_ok=True)
def block(text,start):
 i=text.index(start);begin=text.index('{',i);depth=0
 for j in range(begin,len(text)):
  if text[j]=='{':depth+=1
  elif text[j]=='}':
   depth-=1
   if not depth:return text[i:j+1]
 raise ValueError(start)
def run(name,cmd):
 r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
 (proof/(name+'.log')).write_text(r.stdout+r.stderr)
 if r.returncode:print(r.stdout+r.stderr)
 r.check_returncode();return r.stdout
sdkpath=release/'tools/display-driver-tests-0846/upstream/esp_lcd_panel_dpi.c';gfxpath=release/'tools/display-driver-tests-0846/upstream/Panel_FrameBufferBase.inl';sdk=sdkpath.read_text();gfx=gfxpath.read_text()
rotation=block(gfx,'  void Panel_FrameBufferBase::setRotation(')+'\n'+block(gfx,'  void Panel_FrameBufferBase::_rotate_pixelcopy(')
sdkstruct=block(sdk,'struct esp_lcd_dpi_panel_t {')+';';sdkdraw=block(sdk,'static esp_err_t dpi_panel_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)\n{');sdkdma=block(sdk,'bool mipi_dsi_dma_trans_done_cb(')
stubs=a.build/'stubs';shutil.copytree(release/'tools/display-tests-0843/stubs',stubs,dirs_exist_ok=True)
platform=stubs/'platform.h';text=platform.read_text()
text=text.replace('using esp_lcd_panel_handle_t=void*;','struct esp_lcd_panel_t {unsigned base;};\nusing esp_lcd_panel_handle_t=esp_lcd_panel_t*;\nesp_lcd_panel_handle_t testDriverHandle();')
text=text.replace('struct esp_lcd_dpi_panel_event_callbacks_t {','struct esp_lcd_dpi_panel_event_callbacks_t { bool (*on_color_trans_done)(esp_lcd_panel_handle_t,esp_lcd_dpi_panel_event_data_t*,void*)=nullptr;')
text=text.replace('_disp_panel_handle=this;','_disp_panel_handle=testDriverHandle();')
text=text.replace('constexpr int ESP_CACHE_MSYNC_FLAG_DIR_C2M=1, ESP_CACHE_MSYNC_FLAG_TYPE_DATA=2;','constexpr int ESP_CACHE_MSYNC_FLAG_DIR_C2M=1, ESP_CACHE_MSYNC_FLAG_TYPE_DATA=2, ESP_CACHE_MSYNC_FLAG_UNALIGNED=4;')
text=text.replace('struct Detail {void* buffer=nullptr;};','struct Detail {void* buffer=nullptr;uint16_t dpi_freq_mhz=80,hsync_back_porch=140,hsync_pulse_width=40,hsync_front_porch=40,vsync_back_porch=20,vsync_pulse_width=4,vsync_front_porch=20;};')
text=text.replace('enum class bus_type_t { bus_dsi };','enum class bus_type_t { bus_dsi,bus_spi };').replace('struct Bus { bus_type_t busType(){return bus_type_t::bus_dsi;} };','struct Bus {bus_type_t type=bus_type_t::bus_dsi;bus_type_t busType(){return type;} };')
text=text.replace('enum board_t {board_M5Tab5};','enum board_t {board_M5Tab5,boardOther};').replace('m5::board_t getBoard(){return m5::board_M5Tab5;}','m5::board_t board=m5::board_M5Tab5;m5::board_t getBoard(){return board;}')
text=text.replace('int getColorDepth()', 'bool getSwapBytes(){return false;} int getColorDepth()');platform.write_text(text)
fixture=a.build/'driver.cpp';fixture.write_text((tests/'driver.cpp').read_text().replace('// M5_ROTATION',rotation).replace('// SDK_STRUCT',sdkstruct).replace('// SDK_DRAW',sdkdraw).replace('// SDK_DMA',sdkdma))
common=['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Wno-unused-parameter','-Wno-missing-field-initializers','-Wno-unused-variable','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
results={}
for name,src,defs in [('baseline',a.baseline,[]),('current',sketch,['-DTAB5_PENDING_MIRROR'])]:
 run(name+'-build',common+defs+['-I'+str(stubs),'-I'+str(src),fixture,src/'Tab5DisplayPresent.cpp',src/'Tab5DisplayCache.cpp','-o',a.build/name]);results[name]=json.loads(run(name+'-run',[a.build/name]));print(name,results[name])
results['pendingMetadataOom']=json.loads(run('current-pending-oom',[a.build/'current','pending-oom']))
run('fallback-build',common+['-I'+str(stubs),'-I'+str(sketch),tests/'fallback.cpp',sketch/'Tab5DisplayPresent.cpp',sketch/'Tab5DisplayCache.cpp','-o',a.build/'fallback'])
results['fallbacks']={mode:json.loads(run('fallback-'+mode,[a.build/'fallback',mode])) for mode in ['cache-oom','presenter-unavailable']}
run('pending-build',common+['-I'+str(sketch),tests/'pending.cpp','-o',a.build/'pending']);results['pending']=json.loads(run('pending-run',[a.build/'pending']))
report={'version':'0.8.48','passed':True,'physicalDeviceTested':False,'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'results':results,'sourceHashes':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sketch.glob('Tab5Display*') if p.name in ['Tab5DisplayPresent.cpp','Tab5DisplayPresent.h','Tab5DisplayCache.cpp','Tab5DisplayCacheRanges.h','Tab5DisplayPendingMirror.h']},'testSourceHashes':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in tests.iterdir() if p.is_file()},'referenceHashes':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [sdkpath,gfxpath]},'limits':['ASan/UBSan; actual production presenter/cache and pinned IDF draw/DMA callback bodies. OS/IRQ/cache primitives are modelled.','No physical FPS, PSRAM bandwidth or visible-flash claim.','Tracking contract requires DirtyRect before each write. An unannounced write after a valid DirtyRect is not necessarily detectable by M5GFX weak cache hook.']}
(release/'tools/verification/display-0848.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
