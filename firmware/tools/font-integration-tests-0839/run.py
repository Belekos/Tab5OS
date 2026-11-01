#!/usr/bin/env python3
"""Exact main font/settings helpers and production CJK rebinding, executed with LVGL and file/memory doubles."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,shutil
p=argparse.ArgumentParser();p.add_argument('--lvgl',required=True,type=Path);p.add_argument('--lvgl-build',required=True,type=Path);p.add_argument('--build',required=True,type=Path);p.add_argument('--final',action='store_true');a=p.parse_args()
here=Path(__file__).resolve().parent;root=here.parent.parent;source=root/'Tab5OS_Arduino_v0.8.39_Ecriture';main=source/'Tab5OS_Arduino_v0.8.39_Ecriture.ino';build=a.build.resolve();build.mkdir(parents=True,exist_ok=True);proof=root/'tools/verification/font-integration-0839';proof.mkdir(parents=True,exist_ok=True)
def function(text,name):
 m=re.search(r'^static [^\n;]+\b'+name+r'\([^\n]*\)\s*\{',text,re.M);assert m,name
 start=m.start();pos=text.index('{',start)+1;depth=1;quote=None;comment=None
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
functions=['fontForIndex','fontSizeForIndex','uiFont','documentFont','keyFont','readerFont','configuredFont','resizeConfiguredFont','loadSmoothFonts','ensureKeyFont','ensureReaderFont','loadRichFontBlob','ensureRichFontSource','richFontFor','uiHeadingFont','refreshDefaultFontButtons','chooseDefaultFont','resetDefaultFont']
font_paths=[source/'sdcard/System/font.ttf']+sorted((source/'sdcard/System/fonts').glob('*.ttf'))
production=[main]+[source/f for f in ['Tab5FontDefaults.h','Tab5FontDefaults.cpp','Tab5FontDefaultsValidation.cpp','Tab5CjkFonts.h','Tab5CjkFonts.cpp','Tab5DocumentText.h']]+font_paths
harness=[p for p in sorted(here.iterdir()) if p.is_file()]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def hashes(paths):return {p.relative_to(root).as_posix():sha(p) for p in paths}
before=hashes(production+harness);text=main.read_text();extracted='\n'.join(function(text,n) for n in functions);(build/'helpers.inc').write_text(extracted)
for name in ['Tab5CjkFonts.h','Tab5CjkFonts.cpp']:shutil.copyfile(source/name,build/name)
flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wno-deprecated-declarations','-Wno-cpp','-DTAB5_FONT_DEFAULTS_HOST_TEST','-DLV_CONF_PATH="'+str(a.lvgl_build.resolve()/'lv_conf.h')+'"','-I'+str(build),'-I'+str(here),'-I'+str(source),'-I'+str(a.lvgl.resolve()),'-I'+str(a.lvgl.resolve()/'include')]
cmd=['g++',*flags,str(here/'integration.cpp'),str(build/'Tab5CjkFonts.cpp'),str(source/'Tab5FontDefaults.cpp'),str(source/'Tab5FontDefaultsValidation.cpp'),str(a.lvgl_build.resolve()/'liblvgl.a'),'-pthread','-o',str(build/'integration')]
b=subprocess.run(cmd,capture_output=True,text=True);(proof/'build.log').write_text(b.stdout+b.stderr)
if b.returncode:print(b.stdout+b.stderr);raise SystemExit(b.returncode)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
t=subprocess.run([str(build/'integration'),str(source/'sdcard')],capture_output=True,text=True,env=env,timeout=180);log=t.stdout+t.stderr;(proof/'run.log').write_text(log);print(log,end='')
if t.returncode:raise SystemExit(t.returncode)
assert extracted=='\n'.join(function(main.read_text(),n) for n in functions),'Tested helper changed during tests'
after=hashes(production+harness);stable=before==after
checks=int(re.search(r'PASS (\d+)',log).group(1))
report={'version':'0.8.39','passed':True,'physicalDeviceTested':False,'sourceStableDuringTests':stable,'checks':checks,'sourceHashes':{str(p.relative_to(root)):before[str(p.relative_to(root))] for p in production},'testSourceHashes':hashes(harness),'evidenceHashes':hashes(list(sorted(proof.iterdir()))),'extractedFunctions':functions,'helperSha256':hashlib.sha256(extracted.encode()).hexdigest(),'lvglArchiveSha256':sha(a.lvgl_build/'liblvgl.a'),'method':__doc__,'sanitizers':['address','undefined'],'limitations':['Storage, memory admission and file picker are simulated; native LVGL font creation, events and production CJK rebinding are executed.','Settings serialization is reviewed separately; the full firmware boot and storage timing are not executed by this harness.','LeakSanitizer disabled due executor /proc restrictions; loader allocation ownership is tracked in separate module tests.']}
if a.final:
 assert stable,'Global source changed during final tests'
 (root/'tools/verification/font-integration-0839.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
else:(build/'draft-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
