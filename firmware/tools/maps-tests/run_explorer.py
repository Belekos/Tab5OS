"""Production Explorer controllers + actual LVGL menus, with FS/OS boundaries mocked."""
from pathlib import Path
import hashlib,json,os,subprocess,sys
here=Path(__file__).resolve().parent
release=here.parents[1];sketch=release/'Tab5OS_Arduino_v0.8.26_Ecriture'
build=Path('C:/workspace/tab5-explorer-0826');build.mkdir(exist_ok=True)
if not os.environ.get('VSCMD_VER'):
    command=build/'run.cmd'
    command.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'"\n',encoding='utf8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(command)]))
def block(text,signature):
    begin=text.index(signature);brace=text.index('{',begin);end=brace+1;depth=1
    while depth:
        if text[end]=='{':depth+=1
        elif text[end]=='}':depth-=1
        end+=1
    return text[begin:end]
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl');library=Path('C:/workspace/tab5-ui-0819-host/lvgl.lib')
flags=['/nologo','/utf-8','/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_LABEL_TEXT_SELECTION=1','/DLV_USE_OS=0','/DLV_FONT_MONTSERRAT_20=1','/DLV_FONT_MONTSERRAT_24=1','/D_CRT_SECURE_NO_WARNINGS','/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch)]
main=(sketch/(sketch.name+'.ino')).read_text(encoding='utf8')
flags.append('/I'+str(lvgl/'src'))
source=(here/'explorer.cpp').read_text(encoding='utf8')
results=[]
for mode in ["baseline", "image-baseline", "current"]:
    baseline=mode=="baseline"
    image_baseline=mode=="image-baseline"
    tested=release.parent/'Tab5OS_Ecriture_v0.8.22/Tab5OS_Arduino_v0.8.22_Ecriture/Tab5OS_Arduino_v0.8.22_Ecriture.ino' if baseline else sketch/(sketch.name+'.ino')
    controller=tested.read_text(encoding='utf8')
    pieces={'SELECTED_GIT':block(main,'static bool selectedFolderIsGit() {'),
      'CLOSE_DIALOG':block(controller,'static void closeTextDialog() {'),
      'CLOSE_POPUP':block(controller,'static void closeFilePopup() {'),
      'MKDIR':block(controller,'else if (dialogAction == TextDialogAction::NewFolder) {').split('{',1)[1][:-1],
      'MENUS':(sketch/'Tab5ExplorerMenus.inc').read_text(encoding='utf8'),
      'TOOLBAR':block(main,'static void createFileToolbar(lv_obj_t* parent) {'),
      'PARENT_ICON':block(main,'static lv_obj_t* makeParentFolderButton(lv_obj_t* parent) {'),
      'ROW_PRESS':block(main,'static void fileEntryPressEvent(lv_event_t* e) {'),
      'ROW_CLICK':block(main,'static void fileEntryClickEvent(lv_event_t* e) {'),
      'ROW_DOUBLE_CLICK':block(main,'static void fileEntryDoubleClickEvent(lv_event_t* e) {'),
      'ROW_ACTIVATE':block(main,'static void fileEntryActivateEvent(lv_event_t* e) {'),
      'OPEN_SHELL':block(main,'static void openSelectedShellScript() {'),
      'SELECTED_OPEN':block(main,'static void selectedOpenEvent(lv_event_t* e) {'),
      'SELECTED_EXECUTE':block(main,'static void selectedExecuteEvent(lv_event_t* e) {'),
      'IMAGE_SELECTION':block(main,'} else if (selectedIsImage) {').split('{',1)[1][:-1]}
    pieces['FREE_IMAGE']=block(main,'static void freeFilePreviewImage() {')
    pieces['SHOW_IMAGE']=block(main,'static void showFilePreviewImage() {')
    if image_baseline:
        old=release.parent/'Tab5OS_Ecriture_v0.8.25/Tab5OS_Arduino_v0.8.25_Ecriture/Tab5OS_Arduino_v0.8.25_Ecriture.ino'
        pieces['ROW_ACTIVATE']=block(old.read_text(encoding='utf8'),'static void fileEntryActivateEvent(lv_event_t* e) {')
    candidate=source
    for key,value in pieces.items():candidate=candidate.replace('/* '+key+' */',value)
    cpp=build/('explorer-'+mode+'.cpp');cpp.write_text(candidate,encoding='utf8')
    exe=cpp.with_suffix('.exe')
    cmd=['cl',*flags,'/EHsc','/std:c++17',str(cpp),*[str(sketch/name) for name in ['Tab5Focus.cpp','Tab5TextInput.cpp','Tab5UiStyle.cpp','Tab5ContextMenu.cpp','WriterDocument.cpp']],str(lvgl/'src/font/lv_font_montserrat_20.c'),str(lvgl/'src/font/lv_font_montserrat_24.c'),str(library),'/Fe:'+str(exe)]
    c=subprocess.run(cmd,cwd=build,capture_output=True);log=c.stdout.decode('utf8','replace')+c.stderr.decode('utf8','replace');cpp.with_suffix('.log').write_text(log,encoding='utf8')
    if c.returncode:print(log);raise SystemExit(c.returncode)
    r=subprocess.run([str(exe)]+(['--'+mode] if mode!='current' else []),cwd=build,capture_output=True,text=True,encoding='utf8',timeout=40)
    print(r.stdout+r.stderr)
    results.append({'baseline':baseline,'imageBaseline':image_baseline,'exitCode':r.returncode,'output':r.stdout+r.stderr,'controllerSha256':hashlib.sha256(tested.read_bytes()).hexdigest(),'extractedControllers':{k:hashlib.sha256(v.encode()).hexdigest() for k,v in pieces.items()}})
    if image_baseline:
        results[-1]['imageBaselineSourceSha256']=hashlib.sha256(old.read_bytes()).hexdigest()
report={'version':'0.8.23','passed':all(r['exitCode']==0 for r in results),'physicalDeviceTested':False,'method':'Actual LVGL 9.6.0 and production Focus/TextInput/UiStyle/ContextMenu; complete Explorer menus plus extracted production closeTextDialog, closeFilePopup, mkdir branch and selected-folder Git check. SD/USB filesystem and other application boundaries mocked. .22 baseline demonstrates two failures: deleting with live modal references, and loading inactive Audio on Explorer mkdir. No claim of reproduced physical reset.','results':results,'sources':{name:hashlib.sha256((sketch/name).read_bytes()).hexdigest() for name in [sketch.name+'.ino','Tab5ExplorerMenus.inc','Tab5GitView.h','Tab5Focus.cpp','Tab5TextInput.cpp','Tab5UiStyle.cpp','Tab5ContextMenu.cpp','Tab5ContextMenu.h','Tab5FileSelection.h']},'tests':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__),here/'explorer.cpp']}}
report['version']='0.8.26'
report['sources']['Tab5FileSort.h']=hashlib.sha256((sketch/'Tab5FileSort.h').read_bytes()).hexdigest()
report['method']+=' .24: real responsive toolbar, six explicit ordering choices, and current-directory terminal command with USB and shell metacharacters.'
report['method']+=' .25: deliberate touch second-tap activation, focus restoration without preview reload, single toolbar terminal menu, leftmost folder-parent control, mouse hover focus and translated shortcut display. Shell dispatch executes production open/execute controllers into the runScript boundary (shell grammar tested separately).'
report['method']+=' .26: actual right-preview controller with bounded decoder boundary, second touch and mouse double click, reuse, click-to-viewer, replacement and decode failure/recovery. .25 activation reproduced separately.'
(release/'tools/verification/explorer-0826.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
raise SystemExit(0 if report['passed'] else 1)
