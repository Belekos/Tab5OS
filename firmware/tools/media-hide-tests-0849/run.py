#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
here=Path(__file__).resolve().parent;root=here.parents[1];sketch=root/'Tab5OS_Arduino_v0.8.49_Ecriture'
def block(text,start):
 p=text.index(start);b=text.index('{',p);depth=0
 for e in range(b,len(text)):
  if text[e]=='{':depth+=1
  elif text[e]=='}':
   depth-=1
   if not depth:return text[p:e+1]
 raise ValueError(start)
main=(sketch/(sketch.name+'.ino')).read_text();audio=(sketch/'Tab5Audio.cpp').read_text();media=(sketch/'Tab5MediaList.cpp').read_text()
fixture=(here/'fixture.cpp').read_text().replace('// PRODUCTION_DESTRUCTOR',block(media,'Tab5MediaList::~Tab5MediaList(')).replace('// PRODUCTION_CATALOGUES',block(audio,'size_t Tab5AudioEngine::releaseIdleCatalogues(')).replace('// PRODUCTION_TRANSITION',block(main,'static size_t releaseHiddenMediaList(')+'\n'+block(main,'static void showPage(Page page) {'))
build=Path(tempfile.mkdtemp(prefix='tab5-media-hide-0849-'));(build/'fixture.cpp').write_text(fixture)
r=subprocess.run(['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fno-pie','-no-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined','-I'+str(sketch),str(build/'fixture.cpp'),'-o',str(build/'test')],text=True,capture_output=True);print(r.stderr);r.check_returncode()
r=subprocess.run([str(build/'test')],text=True,capture_output=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));print(r.stdout+r.stderr,end='');r.check_returncode()
report={'version':'0.8.49',**json.loads(r.stdout),'scope':'Actual showPage including real hidden-media helper, MediaList destructor and Audio.releaseIdleCatalogues. UI object/focus/storage/task boundaries controlled, no LVGL rendering in this fixture. 100 leave/reopen mode cycles plus queued command/storage recovery/state lock/priority veto; ASan/UBSan and explicit buffer count. Other page internals are stubs. No physical device claim.','sourceHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [sketch/(sketch.name+'.ino'),sketch/'Tab5Audio.cpp',sketch/'Tab5MediaList.cpp',here/'fixture.cpp',here/'run.py']}}
(root/'tools/verification/media-hide-0849.json').write_text(json.dumps(report,indent=2)+'\n')
