#!/usr/bin/env python3
"""Production presenter/cache against exact pinned IDF draw and DMA-restart routines."""
from pathlib import Path
import datetime,hashlib,json,os,re,shutil,subprocess,argparse,zipfile
r=Path(__file__).resolve().parents[2];s=next(r.glob('Tab5OS_Arduino_*_Ecriture'));h=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,default=r.parent.parent/'build_0846/display-driver');p.add_argument('--baseline',type=Path,default=r.parent.parent/'Tab5OS_0.8.45_SD_Radio.zip');p.add_argument('--sdk-lib',type=Path,default=r.parent.parent/'build_0842/data/packages/m5stack/tools/esp32p4_es-libs/3.3.9/lib/libesp_lcd.a');p.add_argument('--objdump',type=Path,default=r.parent.parent/'build_0842/data/packages/m5stack/tools/esp-rv32/2601/bin/riscv32-esp-elf-objdump');a=p.parse_args();a.build.mkdir(parents=True,exist_ok=True)
def block(text,start):
 i=text.index(start);a=text.index('{',i);depth=0
 for j in range(a,len(text)):
  if text[j]=='{':depth+=1
  elif text[j]=='}':
   depth-=1
   if not depth:return text[i:j+1]
 raise RuntimeError(start)
sdk=(h/'upstream/esp_lcd_panel_dpi.c').read_text()
struct=block(sdk,'struct esp_lcd_dpi_panel_t {')+';'
draw=block(sdk,'static esp_err_t dpi_panel_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)\n{')
dma=block(sdk,'bool mipi_dsi_dma_trans_done_cb(')
for f in (r/'tools/display-tests-0843/stubs').rglob('*'):
 if f.is_file():o=a.build/'stubs'/f.relative_to(r/'tools/display-tests-0843/stubs');o.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(f,o)
p=a.build/'stubs/platform.h';v=p.read_text().replace('using esp_lcd_panel_handle_t=void*;','struct esp_lcd_panel_t {unsigned base;};\nusing esp_lcd_panel_handle_t=esp_lcd_panel_t*;\nesp_lcd_panel_handle_t testDriverHandle();')
v=v.replace('struct esp_lcd_dpi_panel_event_callbacks_t {','struct esp_lcd_dpi_panel_event_callbacks_t { bool (*on_color_trans_done)(esp_lcd_panel_handle_t,esp_lcd_dpi_panel_event_data_t*,void*)=nullptr;')
v=v.replace('_disp_panel_handle=this;','_disp_panel_handle=testDriverHandle();')
v=v.replace('constexpr int ESP_CACHE_MSYNC_FLAG_DIR_C2M=1, ESP_CACHE_MSYNC_FLAG_TYPE_DATA=2;','constexpr int ESP_CACHE_MSYNC_FLAG_DIR_C2M=1, ESP_CACHE_MSYNC_FLAG_TYPE_DATA=2, ESP_CACHE_MSYNC_FLAG_UNALIGNED=4;')
v=v.replace('struct Detail {void* buffer=nullptr;};','struct Detail {void* buffer=nullptr;uint16_t dpi_freq_mhz=80,hsync_back_porch=140,hsync_pulse_width=40,hsync_front_porch=40,vsync_back_porch=20,vsync_pulse_width=4,vsync_front_porch=20;};')
v=v.replace('enum class bus_type_t { bus_dsi };','enum class bus_type_t { bus_dsi,bus_spi };').replace('struct Bus { bus_type_t busType(){return bus_type_t::bus_dsi;} };','struct Bus {bus_type_t type=bus_type_t::bus_dsi;bus_type_t busType(){return type;} };')
v=v.replace('enum board_t {board_M5Tab5};','enum board_t {board_M5Tab5,boardOther};').replace('m5::board_t getBoard(){return m5::board_M5Tab5;}','m5::board_t board=m5::board_M5Tab5;m5::board_t getBoard(){return board;}')
p.write_text(v)
gfx=(h/'upstream/Panel_FrameBufferBase.inl').read_text()
rotation=block(gfx,'  void Panel_FrameBufferBase::setRotation(')+'\n'+block(gfx,'  void Panel_FrameBufferBase::_rotate_pixelcopy(')
main=(h/'fixture.cpp').read_text().replace('// M5_ROTATION',rotation).replace('// SDK_STRUCT',struct).replace('// SDK_DRAW',draw).replace('// SDK_DMA',dma)
(a.build/'test.cpp').write_text(main)
cmd=['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Wno-unused-variable','-Wno-unused-parameter','-Wno-missing-field-initializers','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-I'+str(a.build/'stubs'),'-I'+str(s),str(a.build/'test.cpp'),str(s/'Tab5DisplayPresent.cpp'),str(s/'Tab5DisplayCache.cpp'),'-o',str(a.build/'test')]
o=r/'tools/verification/display-driver-0846';o.mkdir(exist_ok=True)
baseline=a.build/'baseline';baseline.mkdir(exist_ok=True)
assert hashlib.sha256(a.baseline.read_bytes()).hexdigest()=='46fd5da89ebb693a61d3ab34e2d3f3480a714aff76c095ce339f290ee8641573'
with zipfile.ZipFile(a.baseline) as z:
 for n in z.namelist():
  if '/Tab5OS_Arduino_v0.8.45_Ecriture/Tab5Display' in n and n.endswith(('.cpp','.h')):(baseline/Path(n).name).write_bytes(z.read(n))
basecmd=[str(a.build/'baseline-test') if x==str(a.build/'test') else x.replace(str(s),str(baseline)) for x in cmd]
bc=subprocess.run(basecmd,capture_output=True,text=True);(o/'baseline-build.log').write_text(bc.stdout+bc.stderr);bc.check_returncode()
be=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
br=subprocess.run([str(a.build/'baseline-test')],capture_output=True,text=True,env=be);(o/'baseline-run.log').write_text(br.stdout+br.stderr);print('baseline',br.stdout+br.stderr);br.check_returncode()
baselineMetrics=json.loads(br.stdout)
c=subprocess.run(cmd,capture_output=True,text=True);(o/'build.log').write_text(c.stdout+c.stderr);print(c.stderr[-4000:]);c.check_returncode()
e=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1');x=subprocess.run([str(a.build/'test')],capture_output=True,text=True,env=e);(o/'run.log').write_text(x.stdout+x.stderr);print(x.stdout+x.stderr);x.check_returncode()
metrics=json.loads(x.stdout)
fallbackCmd=[str(h/'fallback.cpp') if q==str(a.build/'test.cpp') else str(a.build/'fallback') if q==str(a.build/'test') else q for q in cmd]
fc=subprocess.run(fallbackCmd,capture_output=True,text=True);(o/'fallback-build.log').write_text(fc.stdout+fc.stderr);fc.check_returncode()
fallbackResults={}
for mode in ['cache-oom','presenter-unavailable']:
 fr=subprocess.run([str(a.build/'fallback'),mode],capture_output=True,text=True,env=e);(o/('fallback-'+mode+'.log')).write_text(fr.stdout+fr.stderr);print(fr.stdout+fr.stderr);fr.check_returncode();fallbackResults[mode]=json.loads(fr.stdout)
metrics['fallbacks']=fallbackResults
sdkBinary={'inspected':False}
if a.sdk_lib.is_file() and a.objdump.is_file():
 for fun in ['mipi_dsi_dma_trans_done_cb','dpi_panel_draw_bitmap']:
  binary=subprocess.run([str(a.objdump),'-dr','--disassemble='+fun,str(a.sdk_lib)],capture_output=True,text=True,check=True);(o/('sdk-'+fun+'.txt')).write_text(binary.stdout)
 sdkBinary={'inspected':True,'name':str(a.sdk_lib),'sha256':hashlib.sha256(a.sdk_lib.read_bytes()).hexdigest(),'sourceVersion':'v5.5.4 735507283d','note':'Read-only disassembly agrees with DMA callback ordering; not a complete binary/source equivalence proof.'}
assert metrics['repeatedFullFrameCacheBytes']<baselineMetrics['repeatedFullFrameCacheBytes']
report={'baseline':baselineMetrics,'baselineZipSha256':hashlib.sha256(a.baseline.read_bytes()).hexdigest(),'version':'0.8.46','passed':True,'physicalDeviceTested':False,'sdkBinary':sdkBinary,**metrics,'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'sourceHashes':{str(f.relative_to(r)):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(s.glob('Tab5Display*'))},'testSourceHashes':{str(f.relative_to(r)):hashlib.sha256(f.read_bytes()).hexdigest() for f in [*h.rglob('*'),*(r/'tools/display-tests-0843/stubs').rglob('*')] if f.is_file()},'evidenceHashes':{str(f.relative_to(r)):hashlib.sha256(f.read_bytes()).hexdigest() for f in o.rglob('*') if f.is_file()},'sdk':{'ref':'735507283d','blob':'74c95b4a715c495654a40c0588684fd0dea9ab76','url':'https://github.com/espressif/esp-idf/blob/735507283d/components/esp_lcd/dsi/esp_lcd_panel_dpi.c','methods':{'M5GFX_setRotation_and_rotatePixelcopy':hashlib.sha256(rotation.encode()).hexdigest(),'dpi_panel_draw_bitmap':hashlib.sha256(draw.encode()).hexdigest(),'mipi_dsi_dma_trans_done_cb':hashlib.sha256(dma.encode()).hexdigest()}},'scope':'Exact production presenter/cache and exact pinned IDF draw_bitmap/DMA restart + M5GFX setRotation/rotate_pixelcopy functions; register/link-list/DMA events, cache, memory, tasks and M5 geometry are explicit host adapters. No actual memory bandwidth or physical frame timing measured.','limitations':['No physical panel, bus contention, underrun or visible flash test.','No timing change inferred from host event ordering.','ASan/UBSan active, LSan unavailable in execution environment.']}
(r/'tools/verification/display-driver-0846.json').write_text(json.dumps(report,indent=2)+'\n')
