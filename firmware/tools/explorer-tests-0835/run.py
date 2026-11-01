#!/usr/bin/env python3
"""Exercise exact Explorer selection/menu functions, real selection/model, and image catalog."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,tempfile
here=Path(__file__).resolve().parent
release=here.parents[1];sketch=release/'Tab5OS_Arduino_v0.8.35_Ecriture'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--lvgl',type=Path,default=release.parents[1]/'build_0835/host-dependencies/lvgl')
parser.add_argument('--lvgl-build',type=Path,default=release.parents[1]/'host_lvgl_0835')
args=parser.parse_args()
main=sketch/'Tab5OS_Arduino_v0.8.35_Ecriture.ino';menus=sketch/'Tab5ExplorerMenus.inc'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def function(text,name):
 import re
 match=re.search(r'^static [^\n;]+\b'+name+r'\([^\n]*\)\s*\{',text,re.M)
 assert match,name
 start=match.start();p=text.index('{',match.start());depth=1;p+=1
 quote=None;comment=None
 while depth:
  c=text[p];n=text[p:p+2]
  if comment=='line':
   if c=='\n':comment=None
  elif comment=='block':
   if n=='*/':comment=None;p+=1
  elif quote:
   if c=='\\':p+=1
   elif c==quote:quote=None
  elif n=='//':comment='line';p+=1
  elif n=='/*':comment='block';p+=1
  elif c in ['"',"'"]:quote=c
  elif c=='{':depth+=1
  elif c=='}':depth-=1
  p+=1
 return text[start:p]+'\n'
files=[main,menus,*[sketch/name for name in ['Tab5ImageFormats.h','Tab5ExplorerImages.inc','Tab5FileSelection.h','Tab5DirectoryModel.h','Tab5FileSort.h','Tab5ContextMenu.cpp','Tab5ContextMenu.h','Tab5TextInput.cpp','Tab5TextInput.h','Tab5Focus.cpp','Tab5Focus.h','Tab5UiStyle.cpp','Tab5UiStyle.h','WriterDocument.cpp','WriterDocument.h','WriterKeys.h','Tab5ImageCore.c','tools/native-apps/image-editor/image_core.c','tools/native-apps/image-editor/image_core.h','Tab5ImageDecode.h']]]
before={str(p.relative_to(release)):sha(p) for p in files}
text=main.read_text();menu=menus.read_text()
selection='\n'.join(function(text,n) for n in ['focusRevealedFile','selectFolderPreviewEntry','startFileListPage','refreshFileList'])
enums=menu[:menu.index('static void openFileGit')]
menu_functions=enums+'\n'.join(function(menu,n) for n in ['fileCanEditImage','showFileMenu','openSelectedWith','fileContextMenuAt'])
# Wiring checks prevent a correct helper from becoming detached from production input.
assert 'else selectFolderPreviewEntry(path,false);' in function(text,'serviceFolderPreview')
assert 'focusRevealedFile();' in function(text,'serviceFileSelection')
assert 'openSelectedWith(lv_event_get_current_target_obj(e))' in text
assert 'case FileCommand::ViewImage:if(fileCanEditImage())startImageViewer(selectedPath);' in menu
assert 'case FileCommand::EditImage:if(fileCanEditImage())openImageEditor(selectedPath.c_str());' in menu
assert 'Tab5ImageFormats::supported(name.c_str())' in function(text,'isImageExtension')
proof=release/'tools/verification/explorer-0835';proof.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
def run(command,log,execution=False):
 result=subprocess.run([str(x) for x in command],text=True,capture_output=True,env=env if execution else None,timeout=180)
 (proof/log).write_text(result.stdout+result.stderr)
 if result.returncode:print(result.stdout+result.stderr);raise SystemExit(result.returncode)
 return result.stdout
with tempfile.TemporaryDirectory(prefix='tab5-explorer-0835-') as tmp:
 tmp=Path(tmp);(tmp/'selection-extracted.inc').write_text(selection);(tmp/'menus-extracted.inc').write_text(menu_functions)
 flags=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(sketch)]
 run(['c++','-std=c++17',*flags,'-I'+str(tmp),here/'selection.cpp','-o',tmp/'selection'],'compile-selection.log')
 output=run([tmp/'selection'],'selection.log',True);selection_result=json.loads(output.strip().splitlines()[-1]);print(output,end='')
 run(['cc','-std=c11',*flags,'-c',sketch/'Tab5ImageCore.c','-o',tmp/'core.o'],'compile-core.log')
 run(['c++','-std=c++17',*flags,here/'viewer.cpp',tmp/'core.o','-o',tmp/'viewer'],'compile-viewer.log')
 output=run([tmp/'viewer'],'viewer.log',True);viewer_result=json.loads(output.strip().splitlines()[-1]);print(output,end='')
 lvflags=['-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(args.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(args.lvgl.resolve()),'-I'+str(args.lvgl.resolve()/'include')]
 run(['c++','-std=c++17',*flags,*lvflags,here/'context_menu.cpp',sketch/'WriterDocument.cpp',sketch/'Tab5Focus.cpp',args.lvgl_build/'liblvgl.a','-pthread','-o',tmp/'context-menu'],'compile-context-menu.log')
 output=run([tmp/'context-menu'],'context-menu.log',True);print(output,end='')
 match=re.search(r'PASS (\d+) context-menu checks; groups=(\d+); cycles=(\d+); history_bytes=(\d+); LVGL=(\S+);',output)
 assert match
 context_result={'passed':True,'checks':int(match[1]),'groups':int(match[2]),'cycles':int(match[3]),'historyBytes':int(match[4]),'lvglVersion':match[5],'lvglLibrarySha256':sha(args.lvgl_build/'liblvgl.a'),'lvglBuildManifestSha256':sha(args.lvgl_build/'host-lvgl.json')}
assert before=={str(p.relative_to(release)):sha(p) for p in files},'source changed while testing'
report={
 'version':'0.8.35','passed':True,'physicalDeviceTested':False,'sourceStableDuringTests':True,
 'sourceHashes':before,'selection':selection_result,'viewer':viewer_result,'contextMenu':context_result,
 'checks':selection_result['checks']+viewer_result['checks']+context_result['checks'],
 'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],
 'leakValidation':'LeakSanitizer cannot inspect sandbox /proc. Model/UI test objects explicitly destroyed; viewer allocator asserts zero live bytes and reservations after every success/failure. Production text undo history returns to zero.',
 'coverage':['Exact production folder-preview selection, asynchronous list rebuilding, current folder/search state, sorted page reveal and focus',
  'Actual Tab5FileSelection and Tab5DirectoryModel classes',
  'Deferred focus while a context menu is open; cancelled pending focus when selection changes or target disappears',
  'Actual contextual menu eligibility and commands, native image Open With, right-pane context-menu routing',
  'Case-insensitive image format catalog, shared magic detection and extension ambiguity rejection',
  'Real core T5I composite in viewer: original dimensions, visible layers, RGB565 color, display scaling, corruption and allocation/admission failures',
  'WebP viewer routes through bounded display decoder, retaining natural size and releasing temporary RGB565 frame',
  'Real LVGL context menu 17th action dispatch, 32-action scrolling and 33-action refusal; 512 menu lifecycles, text editing and focus'],
 'limitations':['Explorer widget and storage operations are doubles; no physical touch or USB test.',
  'T5I viewer uses production image core with mocked PSRAM/OS admission and M5Canvas storage.',
  'WebP pixel decoder is mocked in this routing test; actual decoder pixel correctness is covered by codec tests.',
  'Context menu tests use production LVGL, ContextMenu, Focus, TextInput and UiStyle; hardware input transport is not exercised.'],
 'evidenceHashes':{str(p.relative_to(release)):sha(p) for p in [*sorted(here.glob('*.py')),*sorted(here.glob('*.cpp')),*sorted(proof.iterdir())] if p.is_file()}
}
out=release/'tools/verification/explorer-0835.json';out.write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n');print(out)
