#!/usr/bin/env python3
"""Verbatim main dialog, keyboard visibility/dispatch/export functions with real LVGL/Focus/TextInput.
No hardware, network, real keyboard device, DMA, or SD timings are simulated as measurements.
"""
from pathlib import Path
import hashlib,json,os,subprocess,zipfile
r=Path(__file__).resolve().parents[2];s=r/'Tab5OS_Arduino_v0.8.50_Ecriture';t=Path(__file__).parent;ws=r.parent.parent
b=ws/'build_0850/maps-input';b.mkdir(parents=True,exist_ok=True)
lvgl=ws/'build_0850/host-deps/lvgl-9.6.0';lib=ws/'build_0850/host-lvgl'
archive=ws/'recovery_source/Tab5OS_0.8.49_Architecture_Memoire_Audio.zip'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def extract(text,signature):
 start=text.index(signature);i=text.index('{',start)
 while ';' in text[start:i]:
  start=text.index(signature,start+len(signature));i=text.index('{',start)
 depth=0;state='code'
 while i<len(text):
  c=text[i]
  if state=='line':
   if c in '\r\n':state='code'
  elif state=='block':
   if text[i:i+2]=='*/':state='code';i+=1
  elif state in ('"',"'"):
   if c=='\\':i+=1
   elif c==state:state='code'
  elif text[i:i+2]=='//':state='line';i+=1
  elif text[i:i+2]=='/*':state='block';i+=1
  elif c in ('"',"'"):state=c
  elif c=='{':depth+=1
  elif c=='}':
   depth-=1
   if not depth:return text[start:i+1]
  i+=1
 raise ValueError(signature)
main=s/(s.name+'.ino');sources=[main,*[s/x for x in ['Tab5Focus.cpp','Tab5Focus.h','Tab5TextInput.cpp','Tab5TextInput.h','WriterDocument.cpp','WriterDocument.h','WriterKeys.h','Tab5UiStyle.cpp','Tab5UiStyle.h']]];before={str(p.relative_to(r)):sha(p) for p in sources}
with zipfile.ZipFile(archive) as z:old=z.read('Tab5OS_Ecriture_v0.8.49/Tab5OS_Arduino_v0.8.49_Ecriture/Tab5OS_Arduino_v0.8.49_Ecriture.ino').decode()
texts={'before':old,'after':main.read_text()};results={};blockhash={}
for mode,text in texts.items():
 w=b/mode;w.mkdir(exist_ok=True)
 blocks={
 'ENUMS':'\n'.join(extract(text,n)+';' for n in ['enum class Page :','enum class TextDialogAction :','enum class KeyboardMode :']),
 'BUTTON':extract(text,'static lv_obj_t* makeButton('),
 'KEYBOARD':extract(text,'static void setKeyboardVisible(bool visible,'),
 'DIALOG':'\n'.join(extract(text,n) for n in ['static void dialogCancelEvent(','static void openTextDialog(TextDialogAction action,','static void closeTextDialog() {']),
 'DISPATCH':'\n'.join(extract(text,n) for n in ['static bool keyboardDialogScope(','static bool dispatchDialogKeyboard(']),
 'EXPORT':extract(text,'static void mapExportEvent(lv_event_t*) {')}
 code=(t/'ui.cpp').read_text()
 for n,v in blocks.items():code=code.replace('/* '+n+' */',v)
 (w/'ui.cpp').write_text(code);blockhash[mode]={n:hashlib.sha256(v.encode()).hexdigest() for n,v in blocks.items()}
 flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-DBEFORE='+str(int(mode=='before')),'-DLV_CONF_PATH="'+str(lib/'lv_conf.h')+'"','-I'+str(s),'-I'+str(lvgl),'-Wno-cpp','-Wno-deprecated-declarations']
 cmd=['g++',*flags,str(w/'ui.cpp'),*[str(s/n) for n in ['Tab5Focus.cpp','Tab5TextInput.cpp','WriterDocument.cpp','Tab5UiStyle.cpp']],str(lib/'liblvgl.a'),'-pthread','-o',str(w/'test')]
 cp=subprocess.run(cmd,capture_output=True,text=True);(w/'build.log').write_text(cp.stdout+cp.stderr)
 if cp.returncode:print(cp.stderr[-9000:]);raise SystemExit(cp.returncode)
 cp=subprocess.run([str(w/'test')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));(w/'run.log').write_text(cp.stdout+cp.stderr);print(mode,cp.stdout,cp.stderr[-4000:]);cp.check_returncode();results[mode]=json.loads(cp.stdout)
assert before=={str(p.relative_to(r)):sha(p) for p in sources},'Production source changed during run; rerun on final sources'
report={'version':'0.8.50','passed':True,'physicalDeviceTested':False,'sanitizers':['ASan','UBSan'],'results':results,'cycles':100,'keyboardHeights':[264,354],'method':__doc__,'sourceHashes':before,'extractedBlockHashes':blockhash,'testSourceHashes':{str(p.relative_to(r)):sha(p) for p in [t/'ui.cpp',t/'run.py']},'baselineArchiveSha256':sha(archive),'lvglArchiveSha256':sha(lib/'liblvgl.a'),'limits':['Virtual keyboard panel dimensions and rebuild are boundary adapters; real OS dialog focus/visibility/lifetime/keypress dispatcher are executed.','Map engine, canvas and SD export boundaries are controlled in memory; no SD timing claims.','LSan disabled in container; ASan/UBSan active.']}
(r/'tools/verification/map-input-paths-0850.json').write_text(json.dumps(report,indent=2)+'\n')
