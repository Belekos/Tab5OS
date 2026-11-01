#!/usr/bin/env python3
"""Execute extracted production modifier publication/accessor with real HidKeys and a mocked RTOS queue."""
from pathlib import Path
import hashlib,json,os,re,subprocess,tempfile
here=Path(__file__).resolve().parent
release=here.parent.parent
sketch=release/'Tab5OS_Arduino_v0.8.36_Ecriture'
proof=release/'tools/verification/usb-modifiers-0836'
proof.mkdir(parents=True,exist_ok=True)
files=[sketch/name for name in ['WriterUsb.h','WriterUsb.cpp','WriterHid.h']]+[here/'run.py',here/'modifiers.cpp']
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
source_hashes={str(path.relative_to(release)):sha(path) for path in files}
source=(sketch/'WriterUsb.cpp').read_text()
def function(signature):
    start=source.index(signature)
    begin=source.index('{',start)
    depth=1;end=begin+1
    while depth:
        if source[end]=='{':depth+=1
        elif source[end]=='}':depth-=1
        end+=1
    return source[start:end]+'\n'
array=re.search(r'^std::atomic<uint8_t> keyboardModifiers\[MaxInterfaces\]\{\};$',source,re.M)
assert array,'modifier state must stay a fixed atomic array'
assert 'uint8_t usbModifiers();' in (sketch/'WriterUsb.h').read_text()
extracted=array[0]+'\n'+function('void publishKeyboard(')+function('uint8_t usbModifiers(')
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
with tempfile.TemporaryDirectory(prefix='tab5-usb-modifiers-0836-') as temp:
    work=Path(temp);(work/'usb-modifiers-production.inc').write_text(extracted)
    command=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(sketch),'-I'+str(work),str(here/'modifiers.cpp'),'-o',str(work/'modifiers')]
    built=subprocess.run(command,text=True,capture_output=True,timeout=120)
    (proof/'build.log').write_text(built.stdout+built.stderr)
    assert built.returncode==0,built.stderr
    run=subprocess.run([str(work/'modifiers')],text=True,capture_output=True,timeout=120,env=env)
    (proof/'run.log').write_text(run.stdout+run.stderr)
    print(run.stdout+run.stderr,end='');assert run.returncode==0
    result=json.loads(run.stdout.strip().splitlines()[-1])
assert result['failures']==0
assert source_hashes=={str(path.relative_to(release)):sha(path) for path in files},'source changed during test'
report={
    'version':'0.8.36','passed':True,'physicalDeviceTested':False,
    'method':__doc__,'sourceStableDuringTests':True,'sourceHashes':source_hashes,
    'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],
    'extractedProductionSha256':hashlib.sha256(extracted.encode()).hexdigest(),
    'checks':result['checks'],'result':result,
    'coverage':['Every 8-bit modifier pattern on all eight HID interface slots',
      'Immediate non-consuming snapshot before any usbPoll or event dequeue',
      'Modifier-only press/release and repeated reads without queue changes',
      'Multiple keyboards, left/right modifiers, disconnects and slot reuse',
      'Queue overflow during press, release and disconnect',
      'Existing game-button publication and queued HidKeys payload'],
    'limitations':['RTOS queue and interface storage mocked; no physical USB hardware test.',
      'This verifies modifier publication and snapshots, not USB descriptor parsing or transport.',
      'Per-interface atomics are checked functionally; no hardware concurrency timing claim.'],
    'evidenceHashes':{str(path.relative_to(release)):sha(path) for path in sorted(proof.iterdir()) if path.is_file()}
}
output=release/'tools/verification/usb-modifiers-0836.json'
output.write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
print(output)
