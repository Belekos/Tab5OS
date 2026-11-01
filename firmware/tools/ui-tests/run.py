"""Compile real LVGL and native input/style on Windows; no device access."""
from pathlib import Path
import hashlib,json,os,re,subprocess,sys,time
release=Path(__file__).resolve().parents[2]
version=re.search(r'v(\d+\.\d+\.\d+)$',release.name).group(1)
tag=version.replace('.','')
sketch=release/('Tab5OS_Arduino_v'+version+'_Ecriture')
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl')
build=Path('C:/workspace/tab5-ui-'+tag+'-host');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    command=build/'run.cmd'
    command.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf-8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(command)]))
flags=['/nologo','/utf-8','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_LABEL_TEXT_SELECTION=1','/DLV_USE_OS=0','/D_CRT_SECURE_NO_WARNINGS','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch),'/I'+str(build)]
source=(sketch/('Tab5OS_Arduino_v'+version+'_Ecriture.ino')).read_text(encoding='utf-8')
functions=[]
for name in ['uiHeadingFont','applyEditorFont','fontMinusEvent','fontPlusEvent']:
    found=re.search(r'static (?:void|const lv_font_t\*) '+name+r'\([^;]*?\)\s*\{',source)
    if not found:raise RuntimeError('Missing production function: '+name)
    end=source.index('\n}',found.end())+2
    functions.append(source[found.start():end])
# The old mutable document font may only remain on document content and preview.
document_uses=[line.strip() for line in source.splitlines() if 'documentFont()' in line and 'static const lv_font_t* documentFont()' not in line]
expected_uses={'const lv_font_t* f = documentFont();','lv_obj_set_style_text_font(filePreviewLabel, documentFont(), 0);','Tab5DocumentText::apply(fontPreview,documentFont());'}
if set(document_uses)!=expected_uses:raise RuntimeError('Unreviewed mutable document font use: '+repr(document_uses))
# The create-file dialog became the shared context menu in .23; its former
# heading disappeared. The six surviving native headings remain independent.
if source.count('uiHeadingFont(), 0)')!=6:raise RuntimeError('Expected six interface titles isolated from document zoom')
(build/'document_font_integration.inc').write_text('\n'.join(functions)+'\n',encoding='utf-8')
def run(args,log):
    with log.open('w',encoding='utf-8') as f:r=subprocess.run(args,stdout=f,stderr=subprocess.STDOUT,cwd=build)
    if r.returncode:print(log.read_text(encoding='utf-8',errors='replace')[-12000:]);raise SystemExit(r.returncode)
if not (build/'lvgl.lib').exists():
    print('Building actual LVGL host library',flush=True)
    sources=[p for p in (lvgl/'src').rglob('*.c') if p.name!='vg_lite_matrix.c']
    response=build/'lvgl.rsp';response.write_text('\n'.join('"'+str(x)+'"' for x in flags+['/std:c11','/MP8','/c']+[str(p) for p in sources]),encoding='utf-8')
    run(['cl','@'+str(response)],build/'lvgl-build.log')
    response.write_text('\n'.join('"'+str(p)+'"' for p in build.glob('*.obj')),encoding='utf-8')
    run(['lib','/nologo','/OUT:'+str(build/'lvgl.lib'),'@'+str(response)],build/'lvgl-lib.log')
print('Building input and style integration checks',flush=True)
font_flags=flags+['/DLV_USE_TINY_TTF=1','/DLV_TINY_TTF_FILE_SUPPORT=0']
run(['cl',*font_flags,'/std:c11','/c',str(lvgl/'src/font/tiny_ttf/lv_tiny_ttf.c')],build/'font-build.log')
run(['cl',*font_flags,'/EHsc','/std:c++17',str(Path(__file__).with_name('native_controls.cpp')),str(sketch/'WriterDocument.cpp'),str(sketch/'Tab5Focus.cpp'),str(build/'lvgl.lib'),str(build/'lv_tiny_ttf.obj'),'/Fe:'+str(build/'native_controls.exe')],build/'test-build.log')
r=subprocess.run([str(build/'native_controls.exe'),str(sketch/'sdcard/System/font.ttf')],capture_output=True,text=True,encoding='utf-8')
print(r.stdout+r.stderr,flush=True)
tested=['Tab5TextInput.cpp','Tab5UiStyle.cpp','Tab5ThemeSchema.h','Tab5ThemeChoices.h','Tab5TopBarLayout.h','Tab5ThemePreview.cpp','Tab5ThemePreview.h','Tab5DocumentText.h','sdcard/System/font.ttf','Tab5OS_Arduino_v'+version+'_Ecriture.ino']
report={'version':version,'passed':r.returncode==0,'exitCode':r.returncode,'output':r.stdout+r.stderr,'method':'MSVC host execution with actual LVGL 9.6.0, native input, four switch colors, real switch preview, theme catalogue, topbar layout and extracted Settings document-font handlers using actual TinyTTF and shipped font.ttf; Writing API and CJK attachment boundaries mocked here and tested separately','hardwareTested':False,'sources':{str(p.relative_to(release)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [*[sketch/name for name in tested],Path(__file__).with_name('native_controls.cpp'),Path(__file__).with_name('theme_controls.h'),Path(__file__).with_name('document_text.h'),Path(__file__)]}}
(release/('tools/verification/ui-controls-'+tag+'.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
raise SystemExit(r.returncode)
