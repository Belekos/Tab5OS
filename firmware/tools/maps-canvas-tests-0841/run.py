#!/usr/bin/env python3
"""Execute exact main render/free functions against pixel-recording canvas adapters.
Compare 0.8.40's repeated full clear with 0.8.41's same-view resume preservation.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,tempfile,zipfile
r=Path(__file__).resolve().parents[2];here=Path(__file__).resolve().parent;s=r/'Tab5OS_Arduino_v0.8.41_Ecriture'
ap=argparse.ArgumentParser();ap.add_argument('--build',type=Path);ap.add_argument('--baseline-zip',type=Path,required=True);ap.add_argument('--proposed',type=Path);ap.add_argument('--final',action='store_true');a=ap.parse_args();w=a.build or Path(tempfile.mkdtemp(prefix='map-canvas-0841-'));w.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def extract(text,signature):
 start=text.index(signature);i=text.index('{',start);depth=0;state='code'
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
main=s/'Tab5OS_Arduino_v0.8.41_Ecriture.ino';paths=[main,s/'Tab5MapWheel.h'];before={str(p.relative_to(r)):sha(p) for p in paths}
current=(a.proposed or main).read_text()
with zipfile.ZipFile(a.baseline_zip) as z:
 baselineName=next(n for n in z.namelist() if n.endswith('/Tab5OS_Arduino_v0.8.40_Ecriture.ino'));baseline=z.read(baselineName).decode()
proof=r/'tools/verification/maps-canvas-0841';proof.mkdir(exist_ok=True,parents=True);result={};functionHashes={}
for fixed,text in ((False,baseline),(True,current)):
 name='fixed' if fixed else 'baseline';sig='static void renderMapViewport(bool refreshTiles, bool preserveCanvas) {' if fixed else 'static void renderMapViewport(bool refreshTiles) {'
 functions='\n'.join(extract(text,x) for x in (sig,'static void freeMapCanvas() {'));functionHashes[name]=hashlib.sha256(functions.encode()).hexdigest()
 src=w/(name+'.cpp');src.write_text((here/'harness.cpp').read_text().replace('/* FUNCTIONS */',functions));exe=w/name
 cmd=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(s),'-DFIXED='+str(int(fixed)),str(src),'-o',str(exe)]
 p=subprocess.run(cmd,capture_output=True,text=True);(proof/(name+'-build.log')).write_text(p.stdout+p.stderr);p.check_returncode()
 p=subprocess.run([str(exe)],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'));output=p.stdout+p.stderr;(proof/(name+'-run.log')).write_text(output);print(name+': '+output);p.check_returncode()
 m=re.search(r'checks=(\d+) PASS resumeFills=(\d+) resumeInvalidations=(\d+) simulatedPartialPixels=(\d+)',output);assert m
 result[name]=dict(checks=int(m[1]),resumeFills=int(m[2]),resumeInvalidations=int(m[3]),simulatedPartialPixels=int(m[4]))
assert before=={str(p.relative_to(r)):sha(p) for p in paths}
assert current.count('renderMapViewport(false, true);')==1
assert '(radioWasActive && !radioActive))) {\n    renderMapViewport(false, true);' in current
report=dict(version='0.8.41',passed=a.final and not a.proposed,draft=not a.final or bool(a.proposed),method=__doc__,physicalDeviceTested=False,sourceHashes=before,testSourceHashes={str(p.relative_to(r)):sha(p) for p in here.iterdir() if p.is_file()},baseline=dict(zipSha256=sha(a.baseline_zip),mainSha256=hashlib.sha256(baseline.encode()).hexdigest(),path=baselineName),extractedFunctionsSha256=functionHashes,result=result,evidenceHashes={str(p.relative_to(r)):sha(p) for p in proof.iterdir() if p.is_file()},limitations=['Actual render/free functions extracted verbatim; Canvas/LVGL/engine/visible-bounds boundaries mocked. Tests partial pixel retention, generation/counters/priority, changes and failure guards. No physical panel timing or tile decoder tested.'])
(r/'tools/verification/maps-canvas-0841.json').write_text(json.dumps(report,indent=2)+'\n')
