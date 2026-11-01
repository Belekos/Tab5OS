"""Execute the production Explorer layout and action-state function with LVGL."""
from pathlib import Path
import argparse, hashlib, json, os, re, subprocess

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
p=argparse.ArgumentParser()
for name in ('sketch','lvgl','lvgl-build','build'):
    p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--baseline',action='store_true')
a=p.parse_args()
s=a.sketch.resolve();w=a.build.resolve();w.mkdir(parents=True,exist_ok=True)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def function(text,name):
    match=re.search(r'^static [^\n;]+\b'+name+r'\([^\n]*\)\s*\{',text,re.M)
    assert match,name
    start=match.start();pos=text.index('{',start)+1;depth=1;quote=None;comment=None
    while depth:
        c=text[pos];pair=text[pos:pos+2]
        if comment=='line':
            if c=='\n':comment=None
        elif comment=='block':
            if pair=='*/':comment=None;pos+=1
        elif quote:
            if c=='\\':pos+=1
            elif c==quote:quote=None
        elif pair=='//':comment='line';pos+=1
        elif pair=='/*':comment='block';pos+=1
        elif c in ['"',"'"]:quote=c
        elif c=='{':depth+=1
        elif c=='}':depth-=1
        pos+=1
    return text[start:pos]+'\n'
main=next(s.glob('*.ino'));text=main.read_text();menus=(s/'Tab5ExplorerMenus.inc').read_text()
sources=[main,*[s/n for n in ['Tab5ExplorerUi.inc','Tab5ExplorerMenus.inc','Tab5ExplorerRowStyle.inc','Tab5Icons.h','Tab5ImageFormats.h','Tab5FileSort.h','Tab5Mouse.cpp','Tab5Mouse.h','Tab5Focus.cpp','Tab5Focus.h','Tab5UiStyle.cpp','Tab5UiStyle.h','Tab5TextInput.cpp','Tab5TextInput.h','WriterDocument.cpp','WriterDocument.h','WriterKeys.h','WriterUsb.h','WriterMouse.h','sdcard/System/font.ttf']]]
before={str(path):sha(path) for path in sources}
helpers=['makeButton','makeIconButton','makeOpenWithButton','makeSearchButton','makeParentFolderButton','makeSortButton','createFileToolbar']
(w/'helpers.inc').write_text('\n'.join(function(text,name) for name in helpers))
state=['setTopActionVisible','setButtonEnabled','setIconButtonSymbol','fileSelectionHasVolume','fileContextDirectory','fileGitDirectory','fileShowGitAction','updateFileActionState']
(w/'state.inc').write_text('\n'.join(function(text,name) for name in state))
(w/'image-eligible.inc').write_text(function(menus,'fileCanEditImage'))
# Keep the actual dispatch guard and two image cases. Other commands belong to
# separate integration tests and must not launch applications from this fixture.
body=function(menus,'fileMenuCommand')
head=body[:body.index('  switch(command){')]
branches=[]
for name in ('ViewImage','EditImage'):
    start=body.index('    case FileCommand::'+name+':')
    end=body.index('\n',start)
    branches.append(body[start:end])
enum=menus[menus.index('enum class FileCommand'):menus.index('\n\nstatic void openFileGit')]
(w/'command.inc').write_text(enum+'\n'+head+'  switch(command){\n'+'\n'.join(branches)+'\n    default:check(false,"unexpected fixture command");\n  }\n}\n')
flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(s),'-I'+str(w),'-I'+str(a.lvgl.resolve())]
if a.baseline:flags+=['-DBASELINE']
cmd=['g++',*flags,str(HERE/'ui.cpp'),*[str(s/n) for n in ('Tab5Focus.cpp','Tab5UiStyle.cpp','Tab5TextInput.cpp','WriterDocument.cpp')],str(a.lvgl_build.resolve()/'liblvgl.a'),'-pthread','-o',str(w/'ui')]
b=subprocess.run(cmd,capture_output=True,text=True);(w/'compile.log').write_text(b.stdout+b.stderr)
if b.returncode:print(b.stdout+b.stderr);raise SystemExit(b.returncode)
run=subprocess.run([str(w/'ui'),str(w),str(s/'sdcard/System/font.ttf')],capture_output=True,text=True,timeout=120,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
(w/'run.log').write_text(run.stdout+run.stderr)
print(run.stdout+run.stderr)
run.check_returncode()
after={str(path):sha(path) for path in sources}
# The main .ino may be edited concurrently in unrelated dock sections. Extracted
# functions are bound separately; final evidence is regenerated after global gel.
assert before==after,'Source changed during test; repeat after gel'
from PIL import Image
for image in w.glob('*.ppm'):
    Image.open(image).save(image.with_suffix('.png'));image.unlink()
report={'passed':True,'baseline':a.baseline,'physicalDeviceTested':False,'method':__doc__,
        'result':json.loads(run.stdout.strip().splitlines()[-1]),'sourceHashes':before,
        'testSourceHashes':{str(path):sha(path) for path in (Path(__file__),HERE/'ui.cpp')},
        'extractedHashes':{path.name:sha(path) for path in w.glob('*.inc')},
        'lvglArchiveSha256':sha(a.lvgl_build/'liblvgl.a'),
        'sanitizers':['address','undefined'],
        'limits':['Real production createFilePage, toolbar helpers and updateFileActionState; actual image eligibility and image command branches.','Only USB transport is substituted; actual Tab5Mouse::service and LVGL run. Launch callbacks record their destination, storage/busy flags are fixtures.','No physical device. Rendering and source linkage do not prove decode or application launch on hardware.']}
(w/'result.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
