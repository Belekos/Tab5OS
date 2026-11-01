from pathlib import Path
import hashlib,json,os,subprocess,sys
here=Path(__file__).resolve().parent;release=here.parent.parent
sketch=release/'Tab5OS_Arduino_v0.8.26_Ecriture'
build=Path('C:/workspace/tab5-maps-0826');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
 cmd=build/'run.cmd';cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8')
 raise SystemExit(subprocess.call(['cmd','/d','/c',str(cmd)]))
def block(text,signature):
 a=text.index(signature);i=text.index('{',a);depth=1;end=i+1
 while depth:
  if text[end]=='{':depth+=1
  elif text[end]=='}':depth-=1
  end+=1
 return text[a:end]
main=sketch/(sketch.name+'.ino');text=main.read_text(encoding='utf-8')
names=['static lv_obj_t* makeButton(', 'static lv_obj_t* createMapOverlay(', 'static void closeMapStylesEvent(', 'static double mapWorldX(', 'static double mapWorldY(', 'static void mapCoordinatesFromWorld(', 'static void mapPointInfo(int x, int y) {', 'static void mapTouchEvent(']
newnames=names+['static void mapInfoEvent(']
controllers={name:block(text,name) for name in newnames}
def button(text):
 a=text.index('  mapInfoButton = makeIconButton(');b=text.index('  mapInfoLabel = lv_label_create(toolbar);',a)
 return text[a:b]
controllers['info button binding']=button(text)
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-ui-0819-host/lvgl.lib')
arduinojson=Path('C:/workspace/tab5-deps-0815/ArduinoJson/src')
flags=['/nologo','/utf-8','/fsanitize=address','/Zi','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_USE_OS=0','/D_CRT_SECURE_NO_WARNINGS','/DLV_FONT_MONTSERRAT_24=1','/DLV_LABEL_TEXT_SELECTION=1','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch),'/I'+str(arduinojson)]
def compile(name,args):
 result=subprocess.run(['cl',*flags,'/EHsc','/std:c++17',*map(str,args),'/Fe:'+str(build/(name+'.exe'))],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace')
 (build/(name+'-compile.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
 if result.returncode:print(result.stdout+result.stderr);raise SystemExit(result.returncode)
def run(name):
 result=subprocess.run([str(build/(name+'.exe'))],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=45)
 print(name,result.stdout+result.stderr);return {'exitCode':result.returncode,'output':result.stdout+result.stderr}
compile('parser',[here/'parser.cpp']);results={'parser':run('parser')}
baseline=release.parent/'Tab5OS_Ecriture_v0.8.25/Tab5OS_Arduino_v0.8.25_Ecriture/Tab5OS_Arduino_v0.8.25_Ecriture.ino'
oldtext=baseline.read_text(encoding='utf-8')
template=(here/'info.cpp').read_text(encoding='utf-8')
for label,source,signatures in [('info-baseline',oldtext,names),('info',text,newnames)]:
 code=template.replace('/* FUNCTIONS */','\n'.join(block(source,name) for name in signatures)).replace('/* BUTTON */',button(source))
 path=build/(label+'.cpp');path.write_text(code,encoding='utf-8')
 compile(label,[path,sketch/'Tab5Focus.cpp',sketch/'Tab5TextInput.cpp',sketch/'WriterDocument.cpp',lvgl/'src/font/lv_font_montserrat_24.c',lvgl/'src/misc/lv_event.c',lvgl/'src/core/lv_obj_event.c',library])
 results[label]=run(label)
passed=results['parser']['exitCode']==0 and results['info']['exitCode']==0 and results['info-baseline']['exitCode']==1 and 'I button opens point information immediately' in results['info-baseline']['output']
paths=[sketch/'Tab5Map.cpp',sketch/'Tab5Map.h',sketch/'Tab5MapSearchParser.h',sketch/'Tab5Focus.cpp',sketch/'Tab5Focus.h',sketch/'Tab5TextInput.cpp',sketch/'WriterDocument.cpp',here/'parser.cpp',here/'info.cpp',here/'run.py']
report={'version':'0.8.26','passed':passed,'physicalDeviceTested':False,'method':'Production filtered JSON decoder with actual ArduinoJson 7.4.3 and allocator-fault injection under MSVC ASan; production information dialog/button callbacks extracted verbatim, real LVGL 9.6 event core and Focus/TextInput/WriterDocument under ASan, background radio/network not emulated. Same I-button assertion fails on immutable .25 controller and succeeds on .26, 200 UI cycles. Native target compile and stack results recorded separately. No physical reset reproduction claim.','results':results,'baseline':{'path':str(baseline),'sha256':hashlib.sha256(baseline.read_bytes()).hexdigest()},'sources':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},'controllers':{key:hashlib.sha256(value.encode()).hexdigest() for key,value in controllers.items()}}
(release/'tools/verification/maps-search-0826.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(0 if passed else 1)
