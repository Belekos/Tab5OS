#!/usr/bin/env python3
"""Run actual main search-state blocks and Mesh map hook through host adapters.
The tested blocks are extracted verbatim; LVGL rendering and hardware remain separate.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[2];sketch=root/'Tab5OS_Arduino_v0.8.41_Ecriture';here=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--build',type=Path);ap.add_argument('--final',action='store_true');a=ap.parse_args();build=a.build or Path(tempfile.mkdtemp(prefix='maps-ui-'));build.mkdir(exist_ok=True,parents=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def extract(text,signature):
 start=text.index(signature);at=text.index('{',start);i=at;depth=0;state='code'
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
   if not depth:return text[start:i+1],text[at:i+1]
  i+=1
 raise ValueError(signature)
main=sketch/'Tab5OS_Arduino_v0.8.41_Ecriture.ino';header=sketch/'Tab5Map.h';before={str(p.relative_to(root)):sha(p) for p in (main,header,sketch/'Tab5MapWheel.h')};m=main.read_text();h=header.read_text()
types='\n'.join(extract(h,s)[0]+';' for s in ('enum class Tab5MapMarkerKind','struct Tab5MapSearchItem {','struct Tab5MapSearchResult {'))
blocks={};functions=[]
for name,signature,wrapper,body in [
 ('leave','if (previousPage == Page::Map && page != Page::Map)', 'void leave(Page previousPage,Page page)',False),
 ('style','static void mapStyleSelectEvent(lv_event_t* event)',None,False),
 ('open-search','static void mapSearchEvent(lv_event_t* event)',None,False),
 ('poll','while (Tab5Map.pollSearch(searchResult))','void poll()',False),
 ('present','if(mapSearchResultsReady&&activePage==Page::Map&&keyboardScope()==mapPage)','void present()',False),
 ('submit','} else if (dialogAction == TextDialogAction::MapSearch)', 'void submit(String value)',True),
 ('mesh-open','meshHooks.openMap=[](uint32_t node,double latitude,double longitude)','void meshOpen(uint32_t node,double latitude,double longitude)',True)]:
  full,b=extract(m,signature);part=b if body else full;blocks[name]=part
  if wrapper:functions.append(wrapper+'{'+('Tab5MapSearchResult searchResult;' if name=='poll' else '')+part+'}')
  else:functions.append(part)
generated=(here/'harness.cpp').read_text().replace('/* TYPES */',types).replace('/* FUNCTIONS */','\n'.join(functions));(build/'test.cpp').write_text(generated)
proof=root/'tools/verification/maps-ui-0841';proof.mkdir(exist_ok=True,parents=True);cmd=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(sketch),str(build/'test.cpp'),'-o',str(build/'test')]
r=subprocess.run(cmd,capture_output=True,text=True);(proof/'build.log').write_text(r.stdout+r.stderr);r.check_returncode()
r=subprocess.run([str(build/'test')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'));(proof/'run.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr);r.check_returncode();assert before=={str(p.relative_to(root)):sha(p) for p in (main,header,sketch/'Tab5MapWheel.h')}
report={'version':'0.8.41','passed':a.final,'draft':not a.final,'checks':int(re.search(r'checks=(\d+) PASS',r.stdout)[1]),'sourceHashes':before,'testSourceHashes':{str(p.relative_to(root)):sha(p) for p in (here/'harness.cpp',here/'run.py')},'evidenceHashes':{str(p.relative_to(root)):sha(p) for p in proof.iterdir() if p.is_file()},'extractedBlockHashes':{name:hashlib.sha256(text.encode()).hexdigest() for name,text in blocks.items()},'generatedHarnessSha256':sha(build/'test.cpp'),'physicalDeviceTested':False,'limits':[__doc__,'Only actual search-state blocks/style/search-event methods/Mesh-open hook are exercised here; full main/LVGL/engine have separate compilation and runtime tests.']}
(root/'tools/verification/maps-ui-0841.json').write_text(json.dumps(report,indent=2)+'\n')
