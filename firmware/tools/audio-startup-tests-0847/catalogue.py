#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,json,os,subprocess,zipfile
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',type=Path,required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;r=here.parents[1];s=next(r.glob('Tab5OS_Arduino_*_Ecriture'));b=r.parents[1]/'build_0847/audio-catalogue';b.mkdir(parents=True,exist_ok=True)
def block(text,prefix):
 start=text.index(prefix);brace=text.index('{',start);depth=0
 for end in range(brace,len(text)):
  if text[end]=='{':depth+=1
  elif text[end]=='}':
   depth-=1
   if not depth:return text[start:end+1]
 raise ValueError(prefix)
with zipfile.ZipFile(a.baseline_zip) as z:baseline=z.read(next(n for n in z.namelist() if n.endswith('/Tab5Audio.cpp'))).decode()
header=(s/'Tab5Audio.h').read_text()
types=block(header,'enum class Tab5AudioCommandType')+';\n'+block(header,'struct Tab5AudioCommand')+';'
results={}
for label,text in [('before',baseline),('after',(s/'Tab5Audio.cpp').read_text())]:
 names=['bool Tab5AudioEngine::startLocalDecoder(', 'bool Tab5AudioEngine::startRadioNow(', 'void Tab5AudioEngine::setVolume(', 'bool Tab5AudioEngine::execPlayTrack(', 'bool Tab5AudioEngine::execPlayStation(', 'void Tab5AudioEngine::drainCommands(', 'void Tab5AudioEngine::handleCommand(']
 names+=['bool Tab5AudioEngine::selectAdjacentTrack(', 'bool Tab5AudioEngine::selectAdjacentStation(']
 if label=='after':names+=['bool Tab5AudioEngine::resumeSelectedStation(']
 methods=block(text,'class Tab5AudioEngine::StateGuard')+';\n'+'\n'.join(block(text,n) for n in names)
 fixture=(here/'catalogue_fixture.cpp').read_text().replace('// COMMAND_TYPES',types).replace('// PRODUCTION_METHODS',methods)
 cpp=b/(label+'.cpp');cpp.write_text(fixture);binary=b/label
 command=['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=address,undefined','-fno-pie','-no-pie','-DPATCHED='+('1' if label=='after' else '0'),'-I'+str(s),str(cpp),'-o',str(binary)]
 process=subprocess.run(command,capture_output=True,text=True);(b/(label+'-build.log')).write_text(process.stdout+process.stderr)
 if process.returncode:print(process.stderr)
 process.check_returncode()
 process=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
 (b/(label+'-run.log')).write_text(process.stdout+process.stderr);print(process.stdout+process.stderr);process.check_returncode();results[label]=json.loads(process.stdout)
report={'version':'0.8.47','passed':True,**results,'sourceHashes':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [s/'Tab5Audio.cpp',s/'Tab5Audio.h',s/'Tab5CatalogueRevision.h']},'testHashes':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__),here/'catalogue_fixture.cpp']},'scope':'Actual StateGuard, drainCommands, handleCommand, execPlayTrack/Station, startLocalDecoder/RadioNow, resumeSelectedStation, selectAdjacentTrack/Station and volume. Queued stale revisions reject, current revisions resolve, intervening radio reorder rejects, resume URL identity survives reorder and removal rejects; adjacent selection resolves moved sources and guards intervening catalogue generations. Native queue transport/network/files/codecs controlled. ASan/UBSan; no physical test.'}
(r/'tools/verification/audio-catalogue-0847.json').write_text(json.dumps(report,indent=2)+'\n')
