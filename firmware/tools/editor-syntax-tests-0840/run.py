#!/usr/bin/env python3
"""Language-isolated lexer, bounded page-context index, workspace migration and real production RichView rendering with LVGL."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,concurrent.futures
p=argparse.ArgumentParser();p.add_argument('--lvgl',type=Path,default=Path('/workspace/scratch/644cc83c43c8/build_0840/host-dependencies/lvgl-9.6.0'));p.add_argument('--lvgl-build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/host_lvgl_0840'));p.add_argument('--build',type=Path,default=Path('/workspace/scratch/644cc83c43c8/build_0840/editor-syntax-tests'));a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parent.parent;s=r/'Tab5OS_Arduino_v0.8.40_Ecriture';w=a.build.resolve();w.mkdir(parents=True,exist_ok=True);proof=r/'tools/verification/editor-syntax-0840';proof.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
paths=[s/n for n in ['WriterSyntax.cpp','WriterSyntax.h','WriterSyntaxWords.inc','WriterSyntaxIndex.cpp','WriterSyntaxIndex.h','WriterWorkspace.cpp','WriterWorkspace.h','WriterView.cpp','WriterView.h','WriterRich.cpp','WriterRich.h','WriterDocument.cpp','WriterDocument.h','WriterAllocator.h','WriterEncoding.cpp','WriterEncoding.h','WriterStorage.cpp','WriterStorage.h','WriterJson.cpp','WriterJson.h','Tab5Writing.cpp','Tab5Writing.h','sdcard/System/font.ttf','lv_conf.h']]
testpaths=[here/n for n in ['run.py','test.cpp','ui.cpp','stubs/esp_heap_caps.h']]+[r/'tools/encoding-editor-tests-0839/test.cpp'];before={str(p.relative_to(r)):sha(p) for p in paths};testhash={str(p.relative_to(r)):sha(p) for p in testpaths}
base=['-std=c++17','-O1','-g','-Wall','-Wextra','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(s)]
core=[str(s/n) for n in ['WriterSyntax.cpp','WriterRich.cpp','WriterDocument.cpp','WriterEncoding.cpp']]
commands={
'native':['g++',*base,'-DARDUINO_ARCH_ESP32','-I'+str(here/'stubs'),str(here/'test.cpp'),*core,str(s/'WriterSyntaxIndex.cpp'),str(s/'WriterWorkspace.cpp'),'-o',str(w/'native')],
'codecs':['g++',*base,str(r/'tools/encoding-editor-tests-0839/test.cpp'),*core,str(s/'WriterStorage.cpp'),str(s/'WriterJson.cpp'),'-o',str(w/'codecs')],
'ui':['g++',*base,'-Wno-deprecated-declarations','-Wno-cpp','-DLV_CONF_PATH="'+str(a.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(a.lvgl.resolve()),'-I'+str(a.lvgl.resolve()/'include'),str(here/'ui.cpp'),*core,str(s/'WriterView.cpp'),str(a.lvgl_build/'liblvgl.a'),'-pthread','-o',str(w/'ui')]
}
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
def compile(item):
 name,cmd=item;b=subprocess.run(cmd,capture_output=True,text=True);(proof/(name+'-build.log')).write_text(b.stdout+b.stderr);assert b.returncode==0,name+': '+b.stdout+b.stderr
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:list(pool.map(compile,commands.items()))
result={}
for name in commands:
 args=[str(w/name)]+([str(proof),str(s/'sdcard/System/font.ttf')] if name=='ui' else [])
 t=subprocess.run(args,capture_output=True,text=True,env=env,timeout=180);(proof/(name+'-run.log')).write_text(t.stdout+t.stderr);print(name+': '+t.stdout+t.stderr,flush=True);assert t.returncode==0,name+' failed';result[name]=json.loads(t.stdout.strip().splitlines()[-1])
from PIL import Image
for ppm in proof.glob('*.ppm'):Image.open(ppm).save(ppm.with_suffix('.png'));ppm.unlink()
# The integration checks cover actual call sites; modal itself is compiled by target firmware checks.
src=(s/'Tab5Writing.cpp').read_text();checks={
'language choice is per-document and bounded':'entry->syntaxLanguage=uint8_t(syntaxChoice)' in src and 'selected>=unsigned(writing::SyntaxLanguage::Count)' in src,
'opening restores per-document choice':'syntaxChoice=writing::SyntaxLanguage(saved.syntaxLanguage)' in src,
'main and reference views receive page context':'if(view.sourceMode)prepareSyntaxView(false)' in src and 'if(refView.sourceMode)prepareSyntaxView(true)' in src,
'background context uses recolor without render reset':'rich.syntaxInitial=state;rich.recolorSyntax()' in src,
'finished context avoids repeated prefix scan':'if(rich.syntaxReady)continue;' in src,
'choice preserves selection by source bytes':'const size_t caret=a->view.text.sourceAt(a->view.caret),anchor=a->view.text.sourceAt(a->view.anchor)' in src and 'a->view.anchor=a->view.text.indexAtSource(anchor)' in src,
'language switch does not modify document data':'a->syntaxChoice=writing::SyntaxLanguage(selected);a->syntaxIndex.reset();' in src,
'direct and text-selection edits invalidate context':src.count('syntaxIndex.edited(document,')==2,
'default language mode is auto':'writing::SyntaxLanguage syntaxChoice=writing::SyntaxLanguage::Auto' in src}
assert all(checks.values()),checks
(proof/'integration.json').write_text(json.dumps(checks,indent=2)+'\n')
stable=before=={str(p.relative_to(r)):sha(p) for p in paths} and testhash=={str(p.relative_to(r)):sha(p) for p in testpaths};assert stable,'source changed during tests'
report=dict(version='0.8.40',passed=True,physicalDeviceTested=False,sourceStableDuringTests=stable,method=__doc__,sourceHashes=before,testSourceHashes=testhash,sanitizers=['address','undefined'],result=result,integration=checks,checks=sum(v['checks'] for v in result.values())+len(checks),lvglArchiveSha256=sha(a.lvgl_build/'liblvgl.a'),evidenceHashes={str(p.relative_to(r)):sha(p) for p in sorted(proof.iterdir()) if p.is_file()},limitations=['Tests execute the production lexer/index/workspace and RichView with real LVGL and shipped TTF; host timing is not tablet timing.','Language selection dialog integration is checked at its source call sites and target compilation, not by a full OS GUI session.','The highlighter is lexical, not a language parser; embedded languages inside markup and interpolation inside template strings are not parsed. Unknown source extensions remain editable as plain source.','Filesystem migration and rollback use deterministic injected in-memory I/O; no physical SD power-loss test. Leak sanitizer unavailable under host process supervision; native production PSRAM allocation count returns to zero.'])
(r/'tools/verification/editor-syntax-0840.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n');print(json.dumps(dict(passed=True,checks=report['checks'],report=str(r/'tools/verification/editor-syntax-0840.json'))))
