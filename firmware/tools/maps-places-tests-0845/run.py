#!/usr/bin/env python3
"""Saved map places: real ArduinoJson and pinned File/VFS sources, injected syscall failures."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,zipfile
ap=argparse.ArgumentParser();ap.add_argument('--baseline-zip',type=Path);ap.add_argument('--build',type=Path);args=ap.parse_args()
here=Path(__file__).resolve().parent;r=here.parent.parent;s=next(r.glob('Tab5OS_Arduino_v*_Ecriture'));out=(args.build or r.parent.parent/'build_0845/maps-places').resolve();out.mkdir(exist_ok=True,parents=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def method(text,name):
 a=text.index(name);i=text.index('{',a);depth=0;state='code'
 while i<len(text):
  c=text[i]
  if state=='line':
   if c in '\r\n':state='code'
  elif state=='comment':
   if text[i:i+2]=='*/':state='code';i+=1
  elif state in ('"',"'"):
   if c=='\\':i+=1
   elif c==state:state='code'
  elif text[i:i+2]=='//':state='line';i+=1
  elif text[i:i+2]=='/*':state='comment';i+=1
  elif c in ('"',"'"):state=c
  elif c=='{':depth+=1
  elif c=='}':
   depth-=1
   if not depth:return text[a:i+1]
  i+=1
 raise ValueError(name)
files=['Tab5Map.cpp','Tab5Map.h','Tab5AtomicFile.h','Tab5Json.h','Tab5VolumeFS.cpp','Tab5VolumeFS.h','Tab5Storage.cpp','Tab5Storage.h','Tab5SystemPath.h'];sourceHashes={str((s/n).relative_to(r)):sha(s/n) for n in files}
jsonPath=r.parent.parent/'build_0845/sketchbook/libraries/ArduinoJson/src'
if not jsonPath.is_dir():jsonPath=r.parent.parent/'build_0844/sketchbook/libraries/ArduinoJson/src'
assert jsonPath.is_dir()
checksCurrent=r'''
 for(int kind=0;kind<6;++kind){
  reset();put(initial);Tab5MapEngine e;CHECK(e.loadPlaces());CHECK(e._placesLoaded&&e._placeCount==2);CHECK(!strcmp(e._places[0].name,"Alice"));const auto before=get();
  if(kind==0)failOpen="/sd/System/map_places.json";
  if(kind==1)failRead=1;
  if(kind==2)failJson=true;
  if(kind==3){failOpen="/sd/System/map_places.json";failStat=EIO;}
  if(kind==4)failClose=1;
  if(kind==5)put("{bad json");
  const auto preserved=get();CHECK(!e.loadPlaces());CHECK(!e._placesLoaded&&e._placeCount==2);CHECK(!strcmp(e._places[1].name,"Bob"));
  CHECK(!e.addPlace("New",50,2,14));CHECK(!e.renamePlace(0,"Changed"));CHECK(!e.removePlace(0));CHECK(!e.savePlaces());
  CHECK(get()==preserved);CHECK(writeOpens==0&&renames==0&&removes==0);CHECK(tab5SdOpenFiles()==0);CHECK(jsonAllocations.empty());
  failOpen.clear();failRead=failStat=failClose=0;failJson=false;if(kind==5)put(initial);
  CHECK(e.addPlace("New",50,2,14));CHECK(e._placeCount==3);CHECK(!strcmp(e._places[0].name,"Alice"));CHECK(!strcmp(e._places[1].name,"Bob"));
  Tab5MapEngine fresh;CHECK(fresh.loadPlaces()&&fresh._placeCount==3);CHECK(!strcmp(fresh._places[2].name,"New"));
  CHECK(fresh.renamePlace(2,"  Étoile\t nord  "));CHECK(!strcmp(fresh._places[2].name,"Étoile  nord"));
  CHECK(fresh.removePlace(2));CHECK(fresh._placeCount==2);CHECK(get().find("Alice")!=std::string::npos);CHECK(get().find("Bob")!=std::string::npos);
 }
 // An absent primary with a valid backup is read-only until explicit mutation.
 reset();put(initial,"/System/map_places.json.bak");{Tab5MapEngine e;CHECK(e.loadPlaces()&&e._placeCount==2);CHECK(renames==0&&writeOpens==0&&removes==0);CHECK(!std::filesystem::exists(testRoot+MAP_PLACES_FILE));CHECK(get("/System/map_places.json.bak")==initial);CHECK(e.addPlace("Third",50,1,10));CHECK(e._placeCount==3);}
 // A backup stat/read failure is not an empty collection; preserve its bytes.
 for(int kind=0;kind<3;++kind){reset();put(initial,"/System/map_places.json.bak");Tab5MapEngine e;if(kind==0){failOpen="/sd/System/map_places.json.bak";failStat=EIO;}if(kind==1)failRead=1;if(kind==2)failOpen="/sd/System/map_places.json.bak";CHECK(!e.loadPlaces());CHECK(!e.addPlace("New",49,2,10));CHECK(get("/System/map_places.json.bak")==initial);CHECK(!writeOpens&&!renames&&!removes);}
 // Fresh storage has a valid empty collection and can save its first place.
 reset();{Tab5MapEngine e;CHECK(e.loadPlaces()&&e._placesLoaded&&e._placeCount==0);CHECK(e.addPlace("First",48,2,5));CHECK(e._placeCount==1);}
 // A valid JSON of the wrong schema cannot erase a prior table/disk document.
 for(const char* invalid:{"{}","{\"places\":\"bad\"}","[]","null"}){reset();put(initial);Tab5MapEngine e;CHECK(e.loadPlaces());put(invalid);CHECK(!e.loadPlaces());CHECK(e._placeCount==2);CHECK(!e.addPlace("First",48,2,5));CHECK(get()==invalid);CHECK(!writeOpens&&!renames&&!removes);}
 // The supported empty-array schema stays valid, as do bounded UTF-8 names.
 reset();put("{\"places\":[]}");{Tab5MapEngine e;CHECK(e.loadPlaces());CHECK(!e._placeCount);std::string name(62,'x');name+="é";CHECK(e.addPlace(name.c_str(),48,2,5));CHECK(strlen(e._places[0].name)==62);CHECK(e.renamePlace(0,"\n Français \r"));CHECK(!strcmp(e._places[0].name,"Français"));CHECK(!e.addPlace("\r\t ",48,2,5));}
 // Lack of storage/ready table does not attempt any save or mutate data.
 reset();put(initial);{Tab5MapEngine e;e._sdAvailable=false;CHECK(!e.loadPlaces());CHECK(!e.addPlace("New",48,2,5));CHECK(get()==initial);CHECK(!writeOpens&&!renames);}
 // A failed explicit publication keeps the earlier committed file and memory.
 reset();put(initial);{Tab5MapEngine e;CHECK(e.loadPlaces());failPublish=1;CHECK(!e.addPlace("New",48,2,5));CHECK(e._placeCount==2);CHECK(get()==initial);failPublish=0;}
'''
checksBaseline=r'''
 for(int kind=0;kind<3;++kind){reset();put(initial);Tab5MapEngine e;CHECK(e.loadPlaces()&&e._placeCount==2);
  if(kind==0)failOpen="/sd/System/map_places.json";if(kind==1)failRead=1;if(kind==2)failJson=true;
  CHECK(!e.loadPlaces());CHECK(e._placeCount==0);
  failOpen.clear();failRead=0;failJson=false;
  CHECK(e.addPlace("New",50,2,14));CHECK(e._placeCount==1);CHECK(get().find("Alice")==std::string::npos);CHECK(get().find("Bob")==std::string::npos);
 }
'''
def run(label,source,new):
 b=out/label;b.mkdir(exist_ok=True)
 for p in here.rglob('*'):
  if p.is_file() and p.suffix in ('.h','.cpp') and '__pycache__' not in p.parts:
   target=b/p.relative_to(here);target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target)
 for n in files:
  if n not in ('Tab5Map.cpp','Tab5Map.h','Tab5AtomicFile.h'):shutil.copyfile(source/n,b/n)
 text=(source/'Tab5Map.cpp').read_text();atomic=(source/'Tab5AtomicFile.h').read_text()
 helpers='\n'.join(method(atomic,x) for x in ['inline bool tab5AtomicProbe(', 'inline bool tab5AtomicReplace('])+'\n'+'\n'.join(method(text,x) for x in ['static bool writeFileCooperatively(', 'static bool closeWrittenMapFile(', 'static bool prepareMapTemporary(', 'static bool publishMapFile(', 'static void copyUtf8(']+(['static void copyStoredPlaceName('] if new else []))
 methods='\n'.join(method(text,x) for x in ['bool Tab5MapEngine::loadPlaces(', 'bool Tab5MapEngine::savePlaces(', 'bool Tab5MapEngine::addPlace(', 'bool Tab5MapEngine::renamePlace(', 'bool Tab5MapEngine::removePlace('])
 (b/'main.cpp').write_text((here/'fixture.cpp').read_text().replace('// HELPERS',helpers).replace('// METHODS',methods).replace('// SCENARIOS',checksCurrent if new else checksBaseline))
 common=['g++','-std=c++17','-fpermissive','-O1','-g','-pthread','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(b),'-I'+str(b/'sdk'),'-I'+str(jsonPath)]
 objects=[];logs=[]
 for n in ['sdk/vfs_api.cpp','sdk/FS.cpp','Tab5VolumeFS.cpp','Tab5Storage.cpp','main.cpp','host_vfs.cpp']:
  obj=b/(Path(n).stem+'.o');p=subprocess.run(common+['-c',str(b/n),'-o',str(obj)],capture_output=True,text=True);logs.append(p.stdout+p.stderr)
  if p.returncode:raise RuntimeError(''.join(logs))
  objects.append(obj)
 p=subprocess.run(common+[str(x) for x in objects]+['-Wl,--wrap=malloc,--wrap=fopen,--wrap=stat,--wrap=opendir,--wrap=mkdir,--wrap=rename,--wrap=unlink,--wrap=rmdir,--wrap=fread,--wrap=fclose','-o',str(b/'test')],capture_output=True,text=True);logs.append(p.stdout+p.stderr)
 if p.returncode:raise RuntimeError(''.join(logs))
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 p=subprocess.run([str(b/'test'),str(b/'sd')],capture_output=True,text=True,env=env);(b/'build.log').write_text(''.join(logs));(b/'run.log').write_text(p.stdout+p.stderr);print(label,p.stdout+p.stderr)
 if p.returncode:raise RuntimeError(label+' failed')
 return json.loads(p.stdout)
baseline=None
if args.baseline_zip:
 b=out/'baseline-source';b.mkdir(exist_ok=True)
 with zipfile.ZipFile(args.baseline_zip) as z:
  for n in files:(b/n).write_bytes(z.read(next(x for x in z.namelist() if x.endswith('/'+n) and '/Tab5OS_Arduino_' in x and '/tools/' not in x)))
 baseline=run('baseline',b,False)
current=run('current',s,True)
assert sourceHashes=={str((s/n).relative_to(r)):sha(s/n) for n in files},'sources changed'
proof=r/'tools/verification/maps-places-0845';proof.mkdir(exist_ok=True)
for name in ('baseline','current'):
 for log in ('build.log','run.log'):
  p=out/name/log
  if p.is_file():shutil.copyfile(p,proof/(name+'-'+log))
report={'version':'0.8.45','passed':True,'physicalDeviceTested':False,'baseline':baseline,'current':current,'sourceHashes':sourceHashes,'testSourceHashes':{str(p.relative_to(r)):sha(p) for p in here.rglob('*') if p.is_file() and '__pycache__' not in p.parts},'evidenceHashes':{str(p.relative_to(r)):sha(p) for p in proof.glob('*')},'sdkSourceHashes':{str(p.relative_to(here)):sha(p) for p in (here/'sdk').iterdir() if p.is_file()},'arduinoJsonHeaderSha256':sha(jsonPath/'ArduinoJson.h'),'method':__doc__,'limits':['Production place load/save/mutation methods, atomic publication helpers and JSON allocator extracted unchanged. ArduinoJson real; pinned unmodified Arduino File/VFS source with real host POSIX filesystem.','String, Stream, mutex, USB and hardware boundaries adapted; faults injected at actual POSIX fread/fopen/stat/fclose; JSON allocation failures injected via actual allocator interface.','No physical FatFS, SPI, SD timing or tablet resources executed; ASan/UBSan active, LSan disabled (environment process inspection), references/JSON allocations checked explicitly.']}
(r/'tools/verification/maps-places-0845.json').write_text(json.dumps(report,indent=2)+'\n')
