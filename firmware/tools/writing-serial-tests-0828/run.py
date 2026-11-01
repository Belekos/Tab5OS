"""Execute the real Writing toolbar/layout and serial controllers on host LVGL.

Only storage, board serial drivers and unrelated application actions are boundaries.
The Document/RichView model, selection mapping, UI construction, focus, and new
serial session controllers execute production code. This is not the legacy .ino
editor, which is no longer the app users open.
"""
from pathlib import Path
import datetime, hashlib, json, os, re, subprocess, sys

here=Path(__file__).resolve().parent
release=here.parent.parent
sketch=release/'Tab5OS_Arduino_v0.8.28_Ecriture'
build=Path('C:/workspace/tab5-writing-serial-0828');build.mkdir(exist_ok=True)
lvgl=Path('C:/workspace/tab5-deps-0815/lvgl')
library=Path('C:/workspace/tab5-ui-0826-host/lvgl.lib')

def block(text,signature):
    begin=text.index(signature);start=text.index('{',begin)
    depth=0;quote=None;escape=False;line=False;comment=False;i=start
    while i<len(text):
        c=text[i];n=text[i:i+2]
        if line:
            if c=='\n':line=False
        elif comment:
            if n=='*/':comment=False;i+=1
        elif quote:
            if escape:escape=False
            elif c=='\\':escape=True
            elif c==quote:quote=None
        elif n=='//':line=True;i+=1
        elif n=='/*':comment=True;i+=1
        elif c in "\"'":quote=c
        elif c=='{':depth+=1
        elif c=='}':
            depth-=1
            if not depth:return text[begin:i+1]
        i+=1
    raise ValueError(signature)

if not os.environ.get('VSCMD_VER'):
    command=build/'run.cmd'
    command.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n"'+sys.executable+'" -X utf8 "'+str(Path(__file__).resolve())+'" '+ ' '.join(sys.argv[1:])+'\n',encoding='utf8')
    raise SystemExit(subprocess.call(['cmd','/d','/c',str(command)]))

flags=['/nologo','/utf-8','/EHsc','/std:c++17','/Zi','/fsanitize=address',
       '/DLV_CONF_SKIP','/DLV_USE_STDLIB_MALLOC=1','/DLV_LABEL_TEXT_SELECTION=1',
       '/DLV_USE_OS=0','/DLV_FONT_MONTSERRAT_20=1','/DLV_FONT_MONTSERRAT_24=1',
       '/DLV_FONT_MONTSERRAT_32=1','/D_CRT_SECURE_NO_WARNINGS',
       '/I'+str(lvgl),'/I'+str(lvgl/'include'),'/I'+str(sketch),'/I'+str(build)]
signatures=['enum Action {','const char* tr(','uint32_t color(','writing::ViewTheme viewTheme(',
            'static void setLabel(','void caption(','const lv_font_t* font(', 'void panel(',
            'lv_obj_t* button(','lv_obj_t* label(','void build()','void layout()',
            'bool changedRich()','bool replaceSelection(','void closeMenu(','void toggleMenu(']
production=['WriterDocument.cpp','WriterRich.cpp','WriterView.cpp','WriterSyntax.cpp','WriterIndex.cpp',
            'Tab5Focus.cpp','Tab5TextInput.cpp','Tab5ContextMenu.cpp','Tab5UiStyle.cpp']
sources=['Tab5Writing.cpp','Tab5Writing.h','Tab5WritingSerial.inc','Tab5Serial.h','Tab5SerialCore.h',
         'Tab5SerialText.h','WriterDocument.h','WriterRich.h','WriterView.h','WriterIndex.h',*production]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before={name:sha(sketch/name) for name in sources if (sketch/name).exists()}
locale_files=sorted((sketch/'sdcard/System/languages').glob('*.json'))
locale_hashes={p.name:sha(p) for p in locale_files}
captions=['Écouter','Arrêter','Mode Données','Envoyer','Annuler','Insérer','Effacer']
locale_cpp='struct LocaleFixture{const char* id;const char* captions[7];};\nstatic const LocaleFixture locales[]={\n'
for path in locale_files:
    data=json.loads(path.read_text('utf8'))
    if 'translations' not in data:continue
    locale_cpp+='{'+json.dumps(data['id'])+',{'+','.join(json.dumps(data['translations'].get(key,key),ensure_ascii=False) for key in captions)+'}},\n'
locale_cpp+='};\n'
results=[]
modes=['baseline'] if '--baseline-only' in sys.argv else ['current'] if '--current-only' in sys.argv else ['baseline','current']
for mode in modes:
    base=release.parent/'Tab5OS_Ecriture_v0.8.27/Tab5OS_Arduino_v0.8.27_Ecriture' if mode=='baseline' else sketch
    source=(base/'Tab5Writing.cpp').read_text('utf8')
    pieces={sig:block(source,sig) for sig in signatures}
    pieces['enum Action {']+=';'
    for key in ['void Tab5WritingApp::show(','void Tab5WritingApp::resize(']:pieces[key]=block(source,key)
    if True:
        pieces['serial']=(base/'Tab5WritingSerial.inc').read_text('utf8')
        # Prove these controllers are reached by the active app, not an orphan UI.
        assert '#include "Tab5WritingSerial.inc"' in source
        assert 'serialService(now' in block(source,'void Tab5WritingApp::service(')
        assert 'serialStopSession()' in pieces['void Tab5WritingApp::show(']
        begin=source.index('case SerialToggle:');end=source.index('\n',source.index('serialAction(action); break;',begin))
        pieces['serialDispatch']=source[begin:end]
        begin=source.index('case RefClose:');end=source.index('case RefOutline:',begin)
        pieces['serialDispatch']+='\n'+source[begin:end]
    
    generated=(here/'test.cpp').read_text('utf8')
    generated=generated.replace('/* locale fixtures */',locale_cpp)
    for key,value in pieces.items():generated=generated.replace('/* '+key+' */',value)
    cpp=build/('writing-'+mode+'.cpp');cpp.write_text(generated,'utf8')
    exe=build/('writing-'+mode+'.exe')
    cmd=['cl',*flags,*( ['/DBASELINE=1'] if mode=='baseline' else []),str(cpp),
         *[str(sketch/n) for n in production],
         *[str(lvgl/('src/font/lv_font_montserrat_'+str(n)+'.c')) for n in [20,24,32]],
         str(library),'/Fe:'+str(exe)]
    compiled=subprocess.run(cmd,cwd=build,capture_output=True)
    log=(compiled.stdout+compiled.stderr).decode('utf8','replace')
    (build/('compile-'+mode+'.log')).write_text(log,'utf8')
    if compiled.returncode:print(log);raise SystemExit(compiled.returncode)
    runlog=build/('run-'+mode+'.log')
    with runlog.open('wb') as output_file:
        result=subprocess.run([str(exe)],cwd=build,stdout=output_file,stderr=subprocess.STDOUT,timeout=100)
    output=runlog.read_text('utf8',errors='replace');print(output,flush=True)
    results.append(dict(mode=mode,exitCode=result.returncode,output=output,
                        baselineSourceSha256=sha(base/'Tab5Writing.cpp') if mode=='baseline' else None,
                        controllerHashes={k:hashlib.sha256(v.encode('utf8')).hexdigest() for k,v in pieces.items()}))
    if result.returncode:raise SystemExit(result.returncode)

if '--baseline-only' in sys.argv or '--current-only' in sys.argv:raise SystemExit(0)
stable=before=={name:sha(sketch/name) for name in before} and locale_hashes=={p.name:sha(p) for p in locale_files}
report=dict(version='0.8.28',passed=stable and all(r['exitCode']==0 for r in results),
            checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),
            physicalDeviceTested=False,sourceStableDuringTests=stable,sources=before,
            languages=locale_hashes,
            tests={p.name:sha(p) for p in here.glob('*') if p.is_file()},
            method='Real LVGL9.6; production active Tab5Writing build/layout/show/resize, serial include, document changes and selection replacement; production Document, RichView, RichText, index, focus, text input, context and themes. Driver, storage and unrelated editor actions are boundary mocks. C++ controllers compiled with MSVC AddressSanitizer; LVGL archive is prebuilt and uninstrumented. .27 dedicated toolbar and oversized reception caption reproduced separately. Current dropdown, action availability and actual translated captions are exercised.',
            lvglArchiveSha256=sha(library),results=results)
(release/'tools/verification/writing-serial-0828.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n','utf8')
raise SystemExit(0 if report['passed'] else 1)
