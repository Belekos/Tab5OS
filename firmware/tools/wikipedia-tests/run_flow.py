from pathlib import Path
import hashlib,json,os,subprocess,sys
release=Path(__file__).resolve().parents[2]; sketch=release/'Tab5OS_Arduino_v0.8.24_Ecriture'
build=Path('C:/workspace/tab5-wiki-flow-0824');build.mkdir(exist_ok=True)
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-reader-0821/lvgl-target.lib')
if not os.environ.get('VSCMD_VER'):
 command=build/'run.cmd';command.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(command)]))
flags=['/nologo','/utf-8','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_USE_OS=0','/DLV_USE_SPAN=1','/DLV_USE_TINY_TTF=1','/DLV_TINY_TTF_FILE_SUPPORT=0','/D_CRT_SECURE_NO_WARNINGS','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(build),'/I'+str(sketch),'/FI'+str(Path('C:/workspace/tab5-reader-0821/reader-conf.h'))]
def execute(source,tag,current):
 text=(source/'Tab5PythonReader.cpp').read_text(encoding='utf-8')
 (build/'wiki-state.inc').write_text(text[text.index('constexpr size_t WindowSize'):text.index('const char* tr(')],encoding='utf-8')
 code=text[text.index('size_t boundary('):text.index('void status(){')]
 code+=text[text.index('void loadTables(){'):text.index('void loadMedia(){')]
 code+=text[text.index('unsigned charIndex('):text.index('void end(int result,const char* target){')]
 (build/'wiki-functions.inc').write_text(code,encoding='utf-8')
 command=['cl',*flags,'/DCURRENT_FLOW='+str(int(current)),'/EHsc','/std:c++17',str(Path(__file__).with_name('flow.cpp')),str(library),'/Fe:'+str(build/('wiki-'+tag+'.exe'))]
 r=subprocess.run(command,cwd=build,capture_output=True,text=True,encoding='utf-8',errors='replace');(build/(tag+'-build.txt')).write_text(r.stdout+r.stderr,encoding='utf-8')
 if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
 r=subprocess.run([str(build/('wiki-'+tag+'.exe')),str(sketch/'sdcard/System/font.ttf')],capture_output=True,text=True,encoding='utf-8',errors='replace');print(tag,r.stdout+r.stderr)
 return {'passed':r.returncode==0,'exitCode':r.returncode,'output':r.stdout+r.stderr,'sourceSha256':hashlib.sha256((source/'Tab5PythonReader.cpp').read_bytes()).hexdigest()}
baseline=execute(release.parent/'Tab5OS_Ecriture_v0.8.21/Tab5OS_Arduino_v0.8.21_Ecriture','0821',False)
baseline['expectedFailure']=not baseline['passed'] and 'all four cells' in baseline['output']
current=execute(sketch,'0824',True)
paths=[sketch/name for name in ['Tab5PythonReader.cpp','Tab5ReaderLines.h','Tab5WikiTable.h','Tab5Memory.h','sdcard/System/font.ttf','sdcard/System/fonts/cjk-ja.ttf','sdcard/System/fonts/cjk-ko.ttf']]+[Path(__file__),Path(__file__).with_name('flow.cpp')]
report={'version':'0.8.24','passed':current['passed'] and baseline['expectedFailure'],'baseline':baseline,'current':current,'method':'Actual production flow/table/link functions extracted; real LVGL 9.6 TinyTTF target Arabic/BiDi configuration and shipped font; memory-backed files; image decoding replaced by a fixed-height image widget; no physical device','physicalDeviceTested':False,'sources':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}}
(release/'tools/verification/wiki-flow-0824.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');raise SystemExit(0 if report['passed'] else 1)
