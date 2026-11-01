from pathlib import Path
import hashlib,json,os,subprocess,sys,struct,zlib
release=Path(__file__).resolve().parents[2];sketch=release/'Tab5OS_Arduino_v0.8.26_Ecriture'
build=Path('C:/workspace/tab5-calculator-0826');build.mkdir(exist_ok=True)
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-ui-0819-host/lvgl.lib')
if not os.environ.get('VSCMD_VER'):
 command=build/'run.cmd';command.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(command)]))
flags=['/nologo','/utf-8','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_LABEL_TEXT_SELECTION=1','/DLV_USE_OS=0','/DLV_USE_TINY_TTF=1','/DLV_TINY_TTF_FILE_SUPPORT=0','/DLV_FONT_MONTSERRAT_24=1','/DLV_FONT_MONTSERRAT_28=1','/D_CRT_SECURE_NO_WARNINGS','/I'+str(lvgl),'/I'+str(lvgl/'include')]
def run(command,log):
 with (build/log).open('w',encoding='utf-8') as f:result=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,cwd=build)
 if result.returncode:print((build/log).read_text(encoding='utf-8',errors='replace')[-14000:]);raise SystemExit(result.returncode)
run(['cl',*flags,'/std:c11','/c',str(lvgl/'src/font/lv_font_montserrat_24.c'),str(lvgl/'src/font/lv_font_montserrat_28.c'),str(lvgl/'src/font/tiny_ttf/lv_tiny_ttf.c')],'fonts.log')
run(['cl',*flags,'/EHsc','/std:c++17',str(Path(__file__).with_name('layout.cpp')),str(sketch/'Tab5Calculator.cpp'),str(sketch/'Tab5TextInput.cpp'),str(sketch/'Tab5Focus.cpp'),str(sketch/'Tab5UiStyle.cpp'),str(sketch/'WriterDocument.cpp'),str(library),str(build/'lv_font_montserrat_24.obj'),str(build/'lv_font_montserrat_28.obj'),str(build/'lv_tiny_ttf.obj'),'/Fe:'+str(build/'calculator.exe')],'build.log')
try:result=subprocess.run([str(build/'calculator.exe'),str(sketch/'sdcard/System/font.ttf')],cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=30)
except subprocess.TimeoutExpired as error:
 for output in (error.stdout,error.stderr):
  if output:print(output.decode('utf-8',errors='replace') if isinstance(output,bytes) else output)
 raise
print(result.stdout+result.stderr)
images=[]
if result.returncode==0:
 for name in ['calculator-sidebar','calculator-wide','calculator-conversion']:
  source=(build/(name+'.ppm')).read_bytes();pixels=source.split(b'\n',3)[3];assert len(pixels)==1280*720*3
  def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
  png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1280,720,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+pixels[y*1280*3:(y+1)*1280*3] for y in range(720))))+chunk(b'IEND',b'')
  target=release/'tools/verification'/(name+'-0826.png');target.write_bytes(png);images.append(str(target))
report={'version':'0.8.26','physicalDeviceTested':False,'passed':result.returncode==0,'output':result.stdout+result.stderr,'method':'Actual LVGL9.6 pointer hit testing, production history model/view, conversion window, entry continuation, shared geometry/key descriptors and calculator engine; shipped font.ttf rendered by TinyTTF with OS icon fallback; native host renderer; physical device not tested','images':images,'sources':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [sketch/'Tab5CalculatorUi.h',sketch/'Tab5CalculatorHistory.h',sketch/'Tab5CalculatorConversion.h',sketch/'Tab5Calculator.cpp',sketch/'Tab5TextInput.cpp',sketch/'Tab5Focus.cpp',sketch/'Tab5UiStyle.cpp',sketch/'sdcard/System/font.ttf',Path(__file__).with_name('layout.cpp'),Path(__file__)]}}
(release/'tools/verification/calculator-0826.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');raise SystemExit(result.returncode)
