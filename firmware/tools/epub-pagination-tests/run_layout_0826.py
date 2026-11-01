from pathlib import Path
import hashlib,json,os,subprocess,sys
release=Path(__file__).resolve().parents[2];sketch=release/'Tab5OS_Arduino_v0.8.26_Ecriture'
build=Path('C:/workspace/tab5-reader-0826');build.mkdir(exist_ok=True)
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-reader-0825/lvgl-target.lib')
if not os.environ.get('VSCMD_VER'):
 command=build/'run.cmd';command.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8');raise SystemExit(subprocess.call(['cmd','/d','/c',str(command)]))
flags=['/nologo','/utf-8','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_USE_OS=0','/DLV_USE_SPAN=1','/DLV_USE_TINY_TTF=1','/DLV_TINY_TTF_FILE_SUPPORT=0','/D_CRT_SECURE_NO_WARNINGS','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(build),'/I'+str(sketch)]
conf=(sketch/'lv_conf.h').read_text(encoding='utf-8')
matching=['LV_USE_BIDI','LV_BIDI_BASE_DIR_DEF','LV_USE_ARABIC_PERSIAN_CHARS','LV_TXT_BREAK_CHARS','LV_TXT_LINE_BREAK_LONG_LEN']
(build/'reader-conf.h').write_text('\n'.join(line for line in conf.splitlines() if any(line.startswith('#define '+name+' ') for name in matching))+'\n',encoding='utf-8')
flags+=['/FI'+str(build/'reader-conf.h')]
assert (build/'reader-conf.h').read_bytes()==Path('C:/workspace/tab5-reader-0825/reader-conf.h').read_bytes(), 'Reused LVGL host library must have identical text configuration'
def run(command,log):
 with (build/log).open('w',encoding='utf-8') as f:r=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,cwd=build)
 if r.returncode:print((build/log).read_text(encoding='utf-8',errors='replace')[-16000:]);raise SystemExit(r.returncode)
if not library.exists():
 print('Building LVGL with target text shaping/wrap configuration',flush=True)
 sources=[p for p in (lvgl/'src').rglob('*.c') if p.name!='vg_lite_matrix.c']
 response=build/'lvgl.rsp';response.write_text('\n'.join('"'+str(x)+'"' for x in flags+['/std:c11','/MP2','/c']+[str(p) for p in sources]),encoding='utf-8')
 run(['cl','@'+str(response)],'lvgl-build.log')
 response.write_text('\n'.join('"'+str(p)+'"' for p in build.glob('*.obj') if p.name!='layout.obj'),encoding='utf-8')
 run(['lib','/nologo','/OUT:'+str(library),'@'+str(response)],'lvgl-lib.log')
def execute(source,tag):
 text=(source/(source.name+'.ino')).read_text(encoding='utf-8')
 rich=text[text.index('static inline bool richLocEqual('):text.index('static bool readerRichResetPagination()')]
 ensure=text[text.index('static bool readerRichEnsureNextPage(uint16_t page) {'):text.index('static bool readerRenderImage(const String& entryPath) {')]
 navigation=text[text.index('static bool readerGoNextPage() {'):text.index('static bool readerGoPrevPage() {')]
 if '#include "Tab5EpubTextLayout.inc"' in rich:rich=rich.replace('#include "Tab5EpubTextLayout.inc"',(source/'Tab5EpubTextLayout.inc').read_text(encoding='utf-8'))
 (build/'reader.generated.inc').write_text(rich+'\n'+ensure,encoding='utf-8')
 (build/'navigation.generated.inc').write_text(navigation,encoding='utf-8')
 doc=(source/'EpubRender.h').read_text(encoding='utf-8');(build/'document.generated.inc').write_text('namespace epub_render {\n'+doc[doc.index('static constexpr uint16_t MAX_BLOCKS'):doc.index('struct ComputedStyle')]+ '\n}\n',encoding='utf-8')
 run(['cl',*flags,'/EHsc','/std:c++17',str(Path(__file__).with_name('layout.cpp')),str(library),'/Fe:'+str(build/('reader-'+tag+'.exe'))],tag+'-build.log')
 r=subprocess.run([str(build/('reader-'+tag+'.exe')),str(sketch/'sdcard/System/font.ttf')],capture_output=True,text=True,encoding='utf-8',errors='replace');(build/(tag+'-result.txt')).write_text(r.stdout+r.stderr,encoding='utf-8');print(tag,r.stdout+r.stderr);return {'passed':r.returncode==0,'exitCode':r.returncode,'output':r.stdout+r.stderr}
baseline=release.parent/'Tab5OS_Ecriture_v0.8.20/Tab5OS_Arduino_v0.8.20_Ecriture'
old=execute(baseline,'0820') if baseline.exists() else {'notRun':'Previous release is optional and unavailable'}
if 'passed' in old:
 old['expected_failure_observed']=not old['passed'] and 'actual rendered last line fits the page' in old['output']
 old['sourceSha256']=hashlib.sha256((baseline/(baseline.name+'.ino')).read_bytes()).hexdigest()
current=execute(sketch,'0826')
paths=[sketch/(sketch.name+'.ino'),*[sketch/name for name in ['Tab5EpubTextLayout.inc','EpubRender.h','EpubPageIndex.h','Tab5EpubBookIndex.inc','lv_conf.h','sdcard/System/font.ttf','sdcard/System/fonts/cjk-ja.ttf','sdcard/System/fonts/cjk-ko.ttf']],Path(__file__),Path(__file__).with_name('layout.cpp')]
report={'version':'0.8.26','passed':current['passed'],'baseline':old,'current':current,'method':'Extracted actual production pagination/rendering functions; LVGL9.6 with target bidi/Arabic/Persian/line-break configuration; shipped font.ttf plus ja/ko fallback fonts/TinyTTF; mocked document storage/reader shell; no physical device','physicalDeviceTested':False,'sources':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}}
(release/'tools/verification/reader-layout-0826.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');raise SystemExit(0 if current['passed'] else 1)
