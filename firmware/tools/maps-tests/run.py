from pathlib import Path
import hashlib,json,os,struct,subprocess,sys,zlib
here=Path(__file__).resolve().parent;release=here.parent.parent
sketch=release/'Tab5OS_Arduino_v0.8.25_Ecriture'
build=Path('C:/workspace/tab5-maps-0825');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    command=build/'run.cmd'
    command.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(command)]))
def block(text,signature):
    a=text.index(signature);i=text.index('{',a);depth=1;end=i+1
    while depth:
        if text[end]=='{':depth+=1
        elif text[end]=='}':depth-=1
        end+=1
    return text[a:end]
main=sketch/(sketch.name+'.ino');text=main.read_text(encoding='utf-8')
names=['static void clampMapView()','static void syncMapZoomControl()','static void mapZoomSliderEvent(',
       'static double mapWorldX(','static double mapWorldY(','static void mapCoordinatesFromWorld(',
       'static uint8_t mapSourceZoom()','static int mapSourceTileScreenSize()','static void mapVisibleBounds(']
chunks=[block(text,name) for name in names]
source=(here/'view.cpp').read_text(encoding='utf-8').replace('/* FUNCTIONS */','\n'.join(chunks))
(build/'view.cpp').write_text(source,encoding='utf-8')
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-ui-0819-host/lvgl.lib')
flags=['/nologo','/utf-8','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_USE_OS=0','/D_CRT_SECURE_NO_WARNINGS','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch)]
compiled=subprocess.run(['cl',*flags,'/EHsc','/std:c++17',str(build/'view.cpp'),str(sketch/'Tab5TileRaster.cpp'),str(library),'/Fe:'+str(build/'view.exe')],cwd=build,capture_output=True)
log=compiled.stdout.decode('utf-8','replace')+compiled.stderr.decode('utf-8','replace');(build/'compile.log').write_text(log,encoding='utf-8')
if compiled.returncode:print(log);raise SystemExit(compiled.returncode)
ui_names=['static lv_obj_t* makeButton(','static void styleSettingsSlider(','static void styleSettingsDropdown(',
          'static lv_obj_t* createMapOverlay(','static void mapPlacesEvent(lv_event_t* event) {']
ui_chunks=[block(text,name) for name in ui_names]
native=(here/'native-import.cpp').read_text(encoding='utf-8').replace('/* FUNCTIONS */','\n'.join(ui_chunks[:-1])).replace('/* PLACES */',ui_chunks[-1])
(build/'native-import.cpp').write_text(native,encoding='utf-8')
native_flags=flags+['/fsanitize=address','/Zi','/DLV_LABEL_TEXT_SELECTION=1','/DLV_USE_TINY_TTF=1','/DLV_TINY_TTF_FILE_SUPPORT=0','/DLV_FONT_MONTSERRAT_20=1']
compiled=subprocess.run(['cl',*native_flags,'/std:c11','/c',str(lvgl/'src/font/tiny_ttf/lv_tiny_ttf.c')],cwd=build,capture_output=True)
if compiled.returncode:print(compiled.stdout.decode('utf-8','replace'));raise SystemExit(compiled.returncode)
compiled=subprocess.run(['cl',*native_flags,'/EHsc','/std:c++17',str(build/'native-import.cpp'),str(sketch/'Tab5Focus.cpp'),str(sketch/'Tab5TextInput.cpp'),str(sketch/'WriterDocument.cpp'),str(lvgl/'src/font/lv_font_montserrat_20.c'),str(build/'lv_tiny_ttf.obj'),str(lvgl/'src/misc/lv_event.c'),str(lvgl/'src/core/lv_obj_event.c'),str(library),'/Fe:'+str(build/'native-import.exe')],cwd=build,capture_output=True)
log=compiled.stdout.decode('utf-8','replace')+compiled.stderr.decode('utf-8','replace');(build/'native-compile.log').write_text(log,encoding='utf-8')
if compiled.returncode:print(log);raise SystemExit(compiled.returncode)
results=[]
for command in [[str(build/'view.exe')],[str(build/'native-import.exe'),str(sketch/'sdcard/System/font.ttf')],[sys.executable,'-X','utf8',str(here/'imports.py'),str(sketch)]]:
    result=subprocess.run(command,cwd=build,capture_output=True,text=True,encoding='utf-8',timeout=30,env={**os.environ,'TMP':str(build),'TEMP':str(build)})
    results.append({'exitCode':result.returncode,'output':result.stdout+result.stderr});print(result.stdout+result.stderr)
paths=[sketch/'Tab5Map.cpp',sketch/'Tab5Map.h',sketch/'Tab5MapViewPolicy.h',sketch/'Tab5TileRaster.cpp',sketch/'tools/download_t5map.py',sketch/'tools/cartes_tab5.py',sketch/'tools/tab5_map_policy.py',sketch/'sdcard/MicroPython/lib/tab5regions.py',sketch/'sdcard/MicroPython/lib/tab5mappolicy.py']
paths += [sketch/'Tab5MapImportPlan.h',sketch/'Tab5MapImportUi.inc']
paths += [sketch/'Tab5Focus.h',sketch/'Tab5Focus.cpp',sketch/'Tab5TextInput.cpp',sketch/'WriterDocument.cpp']
names += ui_names;chunks += ui_chunks
images=[]
for name in ['map-import-dark','map-import-light']:
    ppm=build/(name+'.ppm')
    if not ppm.exists():continue
    header,dimensions,maximum,data=ppm.read_bytes().split(b'\n',3);width,height=map(int,dimensions.split());assert header==b'P6' and maximum==b'255' and len(data)==width*height*3
    def chunk(kind,payload):return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload))
    scan=b''.join(b'\0'+data[y*width*3:(y+1)*width*3] for y in range(height))
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(scan))+chunk(b'IEND',b'')
    target=release/'tools/verification'/(name+'-0825.png');target.write_bytes(png);images.append(target.name)
report={'version':'0.8.25','passed':all(v['exitCode']==0 for v in results),'physicalDeviceTested':False,
 'method':'Production C++ zoom/coordinate/controller bodies extracted verbatim, actual LVGL slider events and production clipped raster with deterministic decoder; complete production native import dialog/plan/rate limiter, 200 open/import/resume/close cycles with actual Tab5Focus and its keyboard/DELETE callbacks, source policy, clock wrap and cancellation. Native controller, Focus/TextInput/WriterDocument and LVGL event core compiled with MSVC AddressSanitizer; remaining actual LVGL host library reused. Network/storage/clock boundaries mocked. Production PC/Python-tablet importers and PC GUI controller with Tk widgets mocked. No Internet requests; no physical tablet.',
 'results':results,'sources':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
 'controllers':{name:hashlib.sha256(value.encode()).hexdigest() for name,value in zip(names,chunks)},
 'tests':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [here/'run.py',here/'imports.py',here/'view.cpp',here/'native-import.cpp']},'images':images}
(release/'tools/verification/maps-0825.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
