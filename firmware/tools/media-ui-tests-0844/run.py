#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,subprocess,os,sys
from extract import extract,closing
r=Path(__file__).resolve().parents[2];t=Path(__file__).parent;s=r/'Tab5OS_Arduino_v0.8.44_Ecriture';workspace=r.parent.parent
b=workspace/'build_0844/media-ui';b.mkdir(parents=True,exist_ok=True)
base=t/'baseline'
lvgl=workspace/'build_0842/host-dependencies/lvgl-9.6.0';lib=workspace/'host_lvgl_0842'
names=['setLabelIfChanged','setTopActionVisible','setButtonEnabled','setIconButtonSymbol','updateAudioUi']
def block(txt,start):p=txt.index(start);return txt[p:closing(txt,txt.index('{',p))]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
result={}
for mode,folder in [('before',base),('after',s)]:
 w=b/mode;w.mkdir(exist_ok=True)
 if mode=='before':
  structs=(folder/'production-structs.inc').read_text();ui=(folder/'production-ui.inc').read_text();funcs={n:extract(ui,n) for n in names}
 else:
  main=(folder/(folder.name+'.ino')).read_text();audio=(folder/'Tab5Audio.h').read_text();video=(folder/'Tab5Video.h').read_text()
  structs='\n'.join(block(src,key)+';' for src,key in [(audio,'enum class Tab5AudioMode'),(audio,'struct Tab5AudioSnapshot'),(video,'enum class Tab5VideoState'),(video,'struct Tab5VideoSnapshot')]);funcs={n:extract(main,n) for n in names}
 (w/'production-structs.inc').write_text(structs);(w/'production-ui.inc').write_text('\n'.join(funcs.values()))
 flags=['-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-DNEW='+str(int(mode=='after')),'-DLV_CONF_PATH="'+str(lib/'lv_conf.h')+'"','-I'+str(w),'-I'+str(lvgl),'-Wno-cpp','-Wno-deprecated-declarations']
 cmd=['g++',*flags,str(t/'main.cpp'),str(lib/'liblvgl.a'),'-pthread','-o',str(w/'test')]
 cp=subprocess.run(cmd,capture_output=True,text=True);(w/'build.log').write_text(cp.stdout+cp.stderr)
 if cp.returncode:print(cp.stderr[-7000:]);raise SystemExit(cp.returncode)
 cp=subprocess.run([str(w/'test')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));(w/'run.log').write_text(cp.stdout+cp.stderr);print(mode,cp.stdout+cp.stderr)
 if cp.returncode:raise SystemExit(cp.returncode)
 result[mode]={'metrics':json.loads(cp.stdout),'mainFunctions':{n:hashlib.sha256(v.encode()).hexdigest() for n,v in funcs.items()},'structsSha256':hashlib.sha256(structs.encode()).hexdigest()}
 if mode=='after':assert all(extract((folder/(folder.name+'.ino')).read_text(),n)==v for n,v in funcs.items()),'Tested function changed during tests'
o=r/'tools/verification';o.mkdir(exist_ok=True)
report={'version':'0.8.44','passed':True,'physicalDeviceTested':False,'method':'Exact production updateAudioUi and helper functions, real LVGL9.6, controlled Audio/Video/Session and clock boundaries. ASan/UBSan.','limitations':['No MIPI-DSI driver or physical Tab5; unchanged widget calls caused no LVGL flushes already in baseline.','Producer event notification tested separately by root Audio tests.','LSan disabled; ASan/UBSan active.'],'results':result,'sourceHashes':{str((s/(s.name+'.ino')).relative_to(r)):sha(s/(s.name+'.ino'))},'testSourceHashes':{str(p.relative_to(r)):sha(p) for p in t.rglob('*') if p.is_file() and '__pycache__' not in str(p)},'lvglArchiveSha256':sha(lib/'liblvgl.a')}
(o/'media-ui-0844.json').write_text(json.dumps(report,indent=2)+'\n')
