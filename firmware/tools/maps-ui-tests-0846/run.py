#!/usr/bin/env python3
"""Execute exact main callbacks and network-change block; backend/UI boundaries are adapters."""
from pathlib import Path
import argparse, hashlib, json, os, re, subprocess, zipfile

p=argparse.ArgumentParser();p.add_argument('--baseline-zip',required=True,type=Path);p.add_argument('--build',required=True,type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;root=here.parent.parent;sketch=next(root.glob('Tab5OS_Arduino_v*_Ecriture'));build=a.build.resolve();build.mkdir(parents=True,exist_ok=True)
def sha(data):return hashlib.sha256(data).hexdigest()
def extract(text,signature):
    start=text.index(signature);i=text.index('{',start);depth=0;state='code'
    while i<len(text):
        c=text[i]
        if state=='line':
            if c=='\n':state='code'
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
            if not depth:return text[start:i+1]
        i+=1
    raise ValueError(signature)

def prepare(label,files,fixed):
    ino=files['main'].decode().replace('\r','');inc=files['import'].decode().replace('\r','');header=files['audio'].decode().replace('\r','')
    declarations='\n'.join(extract(header,s)+';' for s in ['enum class Tab5AudioMode', 'struct Tab5AudioRoutingSnapshot'])
    functions='\n'.join(extract(ino,s) for s in [
        'static bool mapNetworkConnected() {','static bool mapDownloadsAllowed(',
        'static void audioLocalTabEvent(lv_event_t* e) {','static void audioRadioTabEvent(lv_event_t* e) {',
        'static void audioRecordTabEvent(lv_event_t* e) {','static void audioVideoTabEvent(lv_event_t* e) {',
        'static void audioReloadEvent(lv_event_t* e) {'])+'\n'+extract(inc,'static void mapImportStartEvent(')
    service=extract(ino,'static void serviceMapPage() {')
    marker='  bool connected = false;\n  const bool allowed = mapDownloadsAllowed(&connected);' if fixed else '  Tab5AudioRoutingSnapshot audioState{};\n  const bool haveAudioState'
    block=service[service.rindex(marker):service.rindex('}')]
    source=(here/'ui.cpp').read_text().replace('/* DECLARATIONS */',declarations).replace('/* FUNCTIONS */',functions).replace('/* SERVICE BLOCK */',block)
    target=build/(label+'.cpp');target.write_text(source)
    command=['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror=return-type','-Werror=uninitialized','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-DFIXED='+str(int(fixed)),str(target),'-o',str(build/label)]
    compile=subprocess.run(command,capture_output=True,text=True);(build/(label+'-compile.log')).write_text(compile.stdout+compile.stderr)
    if compile.returncode:raise RuntimeError(compile.stdout+compile.stderr)
    result=subprocess.run([str(build/label)],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
    (build/(label+'-run.log')).write_text(result.stdout+result.stderr);print(label,result.stdout+result.stderr)
    if result.returncode:raise RuntimeError(label+' failed')
    metrics={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',result.stdout)}
    return dict(passed=True,metrics=metrics,extractedFunctionsSha256=sha(functions.encode()),extractedServiceBlockSha256=sha(block.encode()),sourceHashes={k:sha(v) for k,v in files.items()})

assert sha(a.baseline_zip.read_bytes())=='46fd5da89ebb693a61d3ab34e2d3f3480a714aff76c095ce339f290ee8641573','not delivered 0.8.45'
with zipfile.ZipFile(a.baseline_zip) as z:
    prefix='Tab5OS_Ecriture_v0.8.45/Tab5OS_Arduino_v0.8.45_Ecriture/'
    baseline=prepare('baseline',{'main':z.read(prefix+'Tab5OS_Arduino_v0.8.45_Ecriture.ino'),'import':z.read(prefix+'Tab5MapImportUi.inc'),'audio':z.read(prefix+'Tab5Audio.h')},False)
paths={'main':sketch/(sketch.name+'.ino'),'import':sketch/'Tab5MapImportUi.inc','audio':sketch/'Tab5Audio.h'}
files={k:v.read_bytes() for k,v in paths.items()};current=prepare('current',files,True)
assert all(files[k]==v.read_bytes() for k,v in paths.items()),'source changed during test'
report={'version':'0.8.46','passed':True,'method':__doc__,'physicalDeviceTested':False,'sanitizers':['address','undefined'],
    'baseline':baseline,'current':current,'sourceHashes':{str(paths[k].relative_to(root)):sha(v) for k,v in files.items()},
    'testHashes':{str(v.relative_to(root)):sha(v.read_bytes()) for v in [here/'run.py',here/'ui.cpp']},
    'limitations':['LeakSanitizer cannot inspect /proc tasks in this runtime; leak detection disabled. Address/undefined checks remain active.',
    'Only the exact serviceMapPage final network-change block is executed, not full rendering/storage service.',
    'Wi-Fi/audio snapshot/map engine/LVGL action boundaries are adapters. They establish decision/call counts, not radio/SD transport performance.',
    'Actual import starts assert beginGeneration calls and state resets; no tile download or physical display is simulated as success.',
    'Baseline paused-radio gate already allowed by UI; this test does not prove the user physical paused-radio symptom resolved.']}
proof=root/'tools/verification/maps-ui-0846';proof.mkdir(parents=True,exist_ok=True)
for name in ['baseline.cpp','baseline-compile.log','baseline-run.log','current.cpp','current-compile.log','current-run.log']:(proof/name).write_bytes((build/name).read_bytes())
(root/'tools/verification/maps-ui-0846.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
