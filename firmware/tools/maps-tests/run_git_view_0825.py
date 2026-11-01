from pathlib import Path
import hashlib,json,os,struct,subprocess,sys,zlib
here=Path(__file__).resolve().parent;release=here.parent.parent;sketch=release/'Tab5OS_Arduino_v0.8.25_Ecriture'
baseline='--baseline' in sys.argv
build=Path('C:/workspace/tab5-git-view-0825'+('-baseline' if baseline else ''));build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    command=build/'run.cmd';command.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"'+(' --baseline' if baseline else '')+'\n',encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(command)]))
productionPath=(release.parent/'Tab5OS_Ecriture_v0.8.22/Tab5OS_Arduino_v0.8.22_Ecriture/Tab5GitView.cpp') if baseline else sketch/'Tab5GitView.cpp'
production=productionPath.read_text(encoding='utf-8');body=production[production.index('namespace Tab5GitView {'):]
if baseline:body=body.replace('Hooks value){','Hooks value,Start unused){') # Signature-only adapter; unchanged .22 close body.
source=(here/'git-view-0825.cpp').read_text(encoding='utf-8').replace('/* PRODUCTION */',body)
(build/'view.cpp').write_text(source,encoding='utf-8')
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-ui-0819-host/lvgl.lib')
flags=['/fsanitize=address','/Zi']+(['/DTAB5_TEST_BASELINE=1'] if baseline else [])+['/nologo','/utf-8','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_LABEL_TEXT_SELECTION=1','/DLV_USE_OS=0','/DLV_USE_TINY_TTF=1','/DLV_TINY_TTF_FILE_SUPPORT=0','/DLV_FONT_MONTSERRAT_20=1','/DLV_FONT_MONTSERRAT_24=1','/D_CRT_SECURE_NO_WARNINGS','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch)]
result=subprocess.run(['cl',*flags,'/std:c11','/c',str(lvgl/'src/font/tiny_ttf/lv_tiny_ttf.c')],cwd=build,capture_output=True)
if result.returncode:print(result.stdout.decode('utf-8','replace')+result.stderr.decode('utf-8','replace'));raise SystemExit(result.returncode)
cmd=['cl',*flags,'/EHsc','/std:c++17',str(build/'view.cpp'),str(sketch/'Tab5Focus.cpp'),str(sketch/'Tab5TextInput.cpp'),str(sketch/'Tab5UiStyle.cpp'),str(sketch/'Tab5ContextMenu.cpp'),str(sketch/'WriterDocument.cpp'),str(lvgl/'src/font/lv_font_montserrat_20.c'),str(lvgl/'src/font/lv_font_montserrat_24.c'),str(build/'lv_tiny_ttf.obj'),str(lvgl/'src/misc/lv_event.c'),str(lvgl/'src/core/lv_obj_event.c'),str(library),'/Fe:'+str(build/'view.exe')]
result=subprocess.run(cmd,cwd=build,capture_output=True);log=result.stdout.decode('utf-8','replace')+result.stderr.decode('utf-8','replace');(build/'build.log').write_text(log,encoding='utf-8')
if result.returncode:print(log);raise SystemExit(result.returncode)
result=subprocess.run([str(build/'view.exe'),str(sketch/'sdcard/System/font.ttf')],cwd=build,capture_output=True,text=True,encoding='utf-8',timeout=30);print(result.stdout+result.stderr)
if baseline:
    report={'version':'0.8.25','passed':result.returncode!=0 and result.stderr.count('LVGL control: parent actually removed = 1')==25 and 'AddressSanitizer' in result.stderr and ('lv_event_send' in result.stderr or 'event detaches and hides Git immediately' in result.stderr),'physicalDeviceTested':False,'baseline':str(productionPath),'baselineSha256':hashlib.sha256(productionPath.read_bytes()).hexdigest(),'method':'Original .22 GitView body with a signature-only unused Start adapter; real LVGL close-button event and DELETE probe. 25 simple parent-deletion controls pass. Original Git close adds a focus deletion callback to the active close button, then deletes it; ASan reports an event-list use-after-free. Firmware UI retires the tree outside the event and avoids refocusing the closing button. No physical P4 reset claim.','exitCode':result.returncode,'output':result.stdout+result.stderr}
    report['tests']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [here/'git-view-0825.cpp',Path(__file__)]}
    report['sources']={name:hashlib.sha256((sketch/name).read_bytes()).hexdigest() for name in ['Tab5GitView.h','Tab5Git.h','Tab5Focus.cpp','Tab5Focus.h','Tab5UiStyle.cpp','Tab5TextInput.cpp','Tab5ContextMenu.cpp','Tab5ContextMenu.h','Tab5TerminalBuffer.h','WriterDocument.cpp']}
    report['compilation']={'argv':cmd,'exitCode':0,'generatedHarnessSha256':hashlib.sha256((build/'view.cpp').read_bytes()).hexdigest(),'logSha256':hashlib.sha256((build/'build.log').read_bytes()).hexdigest(),'lvglLibrary':{'path':str(library),'sha256':hashlib.sha256(library.read_bytes()).hexdigest()},'instrumentedLvgl':{str(p.relative_to(lvgl)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [lvgl/'src/misc/lv_event.c',lvgl/'src/core/lv_obj_event.c']},'instrumentation':'MSVC /fsanitize=address /Zi. Git, Focus, TextInput, Style, ContextMenu, WriterDocument, event core; remainder from actual LVGL9.6 host library.'}
    (release/'tools/verification/git-close-baseline-0825.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    raise SystemExit(0 if report['passed'] else 1)
images=[]
for name in ['git-view-dark','git-view-settings-light']:
    ppm=build/(name+'.ppm')
    if not ppm.exists():continue
    header,dimensions,maximum,data=ppm.read_bytes().split(b'\n',3);width,height=map(int,dimensions.split());assert header==b'P6' and maximum==b'255' and len(data)==width*height*3
    def chunk(kind,payload):return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload))
    scan=b''.join(b'\0'+data[y*width*3:(y+1)*width*3] for y in range(height))
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(scan))+chunk(b'IEND',b'')
    target=release/'tools/verification'/(name+'-0825.png');target.write_bytes(png);images.append(target.name)
report={'version':'0.8.25','passed':result.returncode==0,'physicalDeviceTested':False,'method':'Complete production Tab5GitView namespace, actual LVGL9.6/OS focus/edit/style, only Git/RTOS/storage boundaries mocked; worker final-access release/acquire barrier, navigation, virtualization and 400 repeated event-driven open/close cycles, deferred tree retirement, text field keyboard deletion callback reentry, model release, clear output and startup Create/Import/Initialize. Ordinary context lifetime is tested, not physical RTOS task deletion. Git engine correctness is covered separately.','result':{'exitCode':result.returncode,'output':result.stdout+result.stderr},'sanitizer':'MSVC AddressSanitizer; production Git/Focus/TextInput/Style/ContextMenu and LVGL event core instrumented; remaining real LVGL library reused','sources':{name:hashlib.sha256((sketch/name).read_bytes()).hexdigest() for name in ['Tab5GitView.cpp','Tab5GitView.h','Tab5Git.h','Tab5Focus.cpp','Tab5UiStyle.cpp','Tab5TextInput.cpp','Tab5TerminalBuffer.h','Tab5ContextMenu.cpp','Tab5ContextMenu.h']},'tests':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [here/'git-view-0825.cpp',Path(__file__)]}}
report['sources']['sdcard/System/font.ttf']=hashlib.sha256((sketch/'sdcard/System/font.ttf').read_bytes()).hexdigest()
report['images']={'files':images,'rendering':'Production LVGL view, actual OS mono base theme plus production palette/styles, delivered font.ttf in TinyTTF 20 px with Montserrat symbol fallback, 1280 px full width without sidebar. Light configuration is cropped to the 432 px available above the OS keyboard; keyboard itself is a boundary stub, not rendered.'}
(release/'tools/verification/git-view-0825.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(result.returncode)
