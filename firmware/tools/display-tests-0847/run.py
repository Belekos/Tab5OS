#!/usr/bin/env python3
"""Native RGB565 transport versus pinned M5GFX and actual IDF display routines."""
from pathlib import Path
import argparse,datetime,hashlib,json,os,shutil,subprocess,zipfile
import re

release=Path(__file__).resolve().parents[2]
sketch=next(release.glob('Tab5OS_Arduino_*_Ecriture'))
tests=Path(__file__).resolve().parent
workspace=release.parents[1]
p=argparse.ArgumentParser()
p.add_argument('--build',type=Path,default=workspace/'build_0847/display')
p.add_argument('--gfx',type=Path,default=workspace/'build_0847/sketchbook/libraries/M5GFX')
p.add_argument('--baseline',type=Path,default=workspace/'baseline_0846/Tab5OS_0.8.46_Reseau_Memoire.zip')
a=p.parse_args();a.build.mkdir(parents=True,exist_ok=True)
proof=release/'tools/verification/display-0847';proof.mkdir(parents=True,exist_ok=True)

def block(text,start):
 i=text.index(start);begin=text.index('{',i);depth=0
 for j in range(begin,len(text)):
  if text[j]=='{':depth+=1
  elif text[j]=='}':
   depth-=1
   if not depth:return text[i:j+1]
 raise ValueError(start)

def run(name,cmd):
 result=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
 (proof/(name+'.log')).write_text(result.stdout+result.stderr)
 if result.returncode:print(result.stdout+result.stderr)
 result.check_returncode();return result.stdout

sdkpath=release/'tools/display-driver-tests-0846/upstream/esp_lcd_panel_dpi.c'
gfxpath=a.gfx/'src/lgfx/v1/panel/Panel_FrameBufferBase.inl'
sdk=sdkpath.read_text();gfx=gfxpath.read_text()
rotation=block(gfx,'  void Panel_FrameBufferBase::setRotation(')+'\n'+block(gfx,'  void Panel_FrameBufferBase::_rotate_pixelcopy(')
write=block(gfx,'  void Panel_FrameBufferBase::writeImage(')
blit=(tests/'blit.cpp').read_text().replace('// M5_METHODS',rotation+'\n'+write)
(a.build/'blit.cpp').write_text(blit)
common=['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Wno-unused-parameter','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
run('blit-build',common+['-I'+str(sketch),'-I'+str(a.gfx/'src'),a.build/'blit.cpp','-o',a.build/'blit'])
blitResult=json.loads(run('blit-run',[a.build/'blit']));print('blit',blitResult)

stubs=a.build/'stubs'
shutil.copytree(release/'tools/display-tests-0843/stubs',stubs,dirs_exist_ok=True)
platform=stubs/'platform.h';text=platform.read_text()
text=text.replace('using esp_lcd_panel_handle_t=void*;','struct esp_lcd_panel_t {unsigned base;};\nusing esp_lcd_panel_handle_t=esp_lcd_panel_t*;\nesp_lcd_panel_handle_t testDriverHandle();')
text=text.replace('struct esp_lcd_dpi_panel_event_callbacks_t {','struct esp_lcd_dpi_panel_event_callbacks_t { bool (*on_color_trans_done)(esp_lcd_panel_handle_t,esp_lcd_dpi_panel_event_data_t*,void*)=nullptr;')
text=text.replace('_disp_panel_handle=this;','_disp_panel_handle=testDriverHandle();')
text=text.replace('constexpr int ESP_CACHE_MSYNC_FLAG_DIR_C2M=1, ESP_CACHE_MSYNC_FLAG_TYPE_DATA=2;','constexpr int ESP_CACHE_MSYNC_FLAG_DIR_C2M=1, ESP_CACHE_MSYNC_FLAG_TYPE_DATA=2, ESP_CACHE_MSYNC_FLAG_UNALIGNED=4;')
text=text.replace('struct Detail {void* buffer=nullptr;};','struct Detail {void* buffer=nullptr;uint16_t dpi_freq_mhz=80,hsync_back_porch=140,hsync_pulse_width=40,hsync_front_porch=40,vsync_back_porch=20,vsync_pulse_width=4,vsync_front_porch=20;};')
text=text.replace('enum class bus_type_t { bus_dsi };','enum class bus_type_t { bus_dsi,bus_spi };').replace('struct Bus { bus_type_t busType(){return bus_type_t::bus_dsi;} };','struct Bus {bus_type_t type=bus_type_t::bus_dsi;bus_type_t busType(){return type;} };')
text=text.replace('enum board_t {board_M5Tab5};','enum board_t {board_M5Tab5,boardOther};').replace('m5::board_t getBoard(){return m5::board_M5Tab5;}','m5::board_t board=m5::board_M5Tab5;m5::board_t getBoard(){return board;}')
text=text.replace('int getColorDepth()', 'bool getSwapBytes(){return false;} int getColorDepth()')
platform.write_text(text)
sdkstruct=block(sdk,'struct esp_lcd_dpi_panel_t {')+';'
sdkdraw=block(sdk,'static esp_err_t dpi_panel_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)\n{')
sdkdma=block(sdk,'bool mipi_dsi_dma_trans_done_cb(')
def generated(fixture):
 return fixture.replace('// M5_ROTATION',rotation).replace('// SDK_STRUCT',sdkstruct).replace('// SDK_DRAW',sdkdraw).replace('// SDK_DMA',sdkdma)
current=a.build/'driver.cpp';current.write_text(generated((tests/'driver.cpp').read_text()))
driverCommand=common+['-Wno-missing-field-initializers','-Wno-unused-variable','-I'+str(stubs),'-I'+str(sketch),current,sketch/'Tab5DisplayPresent.cpp',sketch/'Tab5DisplayCache.cpp','-o',a.build/'driver']
run('driver-build',driverCommand)
driverResult=json.loads(run('driver-run',[a.build/'driver']));print('driver',driverResult)

expected='22d99628cc3ab6a9673be1886755ec9a3092aaede3f017a7e0e20443cc0816a6'
assert hashlib.sha256(a.baseline.read_bytes()).hexdigest()==expected
baseline=a.build/'baseline';baseline.mkdir(exist_ok=True)
with zipfile.ZipFile(a.baseline) as z:
 for n in z.namelist():
  if '/Tab5OS_Arduino_v0.8.46_Ecriture/Tab5Display' in n and n.endswith(('.cpp','.h')):(baseline/Path(n).name).write_bytes(z.read(n))
basefixture=a.build/'baseline-driver.cpp';basefixture.write_text(generated((release/'tools/display-driver-tests-0846/fixture.cpp').read_text()))
baseCommand=[str(q).replace(str(current),str(basefixture)).replace(str(sketch),str(baseline)).replace(str(a.build/'driver'),str(a.build/'baseline-driver')) for q in driverCommand]
run('baseline-build',baseCommand)
baselineResult=json.loads(run('baseline-run',[a.build/'baseline-driver']));print('baseline',baselineResult)

fallbackCommand=[str(tests/'fallback.cpp') if q==current else str(a.build/'fallback') if q==a.build/'driver' else q for q in driverCommand]
run('fallback-build',fallbackCommand)
fallbackResults={mode:json.loads(run('fallback-'+mode,[a.build/'fallback',mode])) for mode in ['cache-oom','presenter-unavailable']}
print('fallback',fallbackResults)

pipelineStubs=a.build/'pipeline-stubs';shutil.copytree(release/'tools/display-tests-0846/stubs',pipelineStubs,dirs_exist_ok=True)
platform=pipelineStubs/'platform.h';text=platform.read_text()
text=text.replace('struct DisplayMock {','void testUnexpectedFallback(int32_t,int32_t,int32_t,int32_t,const uint16_t*);\nstruct DisplayMock {void startWrite(){++panel.count;}void endWrite(){--panel.count;}void pushImage(int32_t x,int32_t y,int32_t w,int32_t h,const uint16_t* p){testUnexpectedFallback(x,y,w,h,p);}')
text=text.replace('int getColorDepth()', 'bool getSwapBytes(){return false;} int getColorDepth()')
platform.write_text(text)
ino=next(sketch.glob('*.ino'));flush=block(ino.read_text(),'static void displayFlush(')
pipeline=a.build/'pipeline.cpp';pipeline.write_text((tests/'pipeline.cpp').read_text().replace('// PRODUCTION_FLUSH',flush))
lvgl=workspace/'build_0847/sketchbook/libraries/lvgl';lvglBuild=workspace/'build_0847/host-lvgl'
run('pipeline-build',common+['-Wno-cpp','-DLV_CONF_PATH="'+str(lvglBuild/'lv_conf.h')+'"','-I'+str(pipelineStubs),'-I'+str(sketch),'-I'+str(lvgl),pipeline,sketch/'Tab5DisplayRefresh.cpp',sketch/'Tab5DisplayPresent.cpp',sketch/'Tab5DisplayCache.cpp',sketch/'Tab5DisplayActivity.cpp',lvglBuild/'liblvgl.a','-pthread','-o',a.build/'pipeline'])
pipelineOutput=run('pipeline-run',[a.build/'pipeline']);print('pipeline',pipelineOutput)
pipelineResult={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',pipelineOutput)}

report={'version':'0.8.47','passed':True,'physicalDeviceTested':False,'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'baselineZipSha256':expected,'blit':blitResult,'driver':driverResult,'baseline':baselineResult,'fallbacks':fallbackResults,'lvglPipeline':pipelineResult,'productionFlushSha256':hashlib.sha256(flush.encode()).hexdigest(),
 'sourceHashes':{str(f.relative_to(release)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(sketch.glob('Tab5Display*'))},
 'testSourceHashes':{str(f.relative_to(release)):hashlib.sha256(f.read_bytes()).hexdigest() for f in tests.rglob('*') if f.is_file()},
 'referenceSources':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in [sdkpath,gfxpath,a.gfx/'src/lgfx/v1/misc/pixelcopy.hpp',a.gfx/'src/lgfx/v1/misc/colortype.hpp']},
 'evidenceHashes':{str(f.relative_to(release)):hashlib.sha256(f.read_bytes()).hexdigest() for f in proof.rglob('*') if f.is_file()},
 'limitations':['Real M5GFX rotation/writeImage/pixelcopy and production blit are executed; DMA/cache/interrupt boundaries are host adapters.','No physical PSRAM bandwidth, FPS, underrun or visible flash conclusion.','ASan and UBSan active; leak sanitizer unavailable in host environment.']}
(release/'tools/verification/display-0847.json').write_text(json.dumps(report,indent=2)+'\n')
