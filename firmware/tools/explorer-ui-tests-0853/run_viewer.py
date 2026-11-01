"""Verify T5I viewer dispatch/rendering and resource cleanup against current sources."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess

HERE=Path(__file__).resolve().parent
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sketch',type=Path,required=True)
p.add_argument('--build',type=Path,required=True)
a=p.parse_args();s=a.sketch.resolve();w=a.build.resolve();w.mkdir(parents=True,exist_ok=True)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
main=next(s.glob('*.ino'))
files=[main,*[s/name for name in ('Tab5ExplorerImages.inc','Tab5ImageFormats.h','Tab5ImageDecode.h','Tab5ImageCore.c','tools/native-apps/image-editor/image_core.c','tools/native-apps/image-editor/image_core.h')]]
before={str(path):sha(path) for path in files}
text=main.read_text();start=text.index('static bool loadSdImageCanvas(',text.index('#include "Tab5ExplorerImages.inc"'));end=text.index('  const size_t len = f.size();',start)
# The exact production opening/signature dispatch is linked through both native
# handlers. The unrelated M5 JPEG/PNG/BMP decoder is outside this host fixture.
(w/'viewer-dispatch.inc').write_text(text[start:end]+'  check(false); return false;\n}\n')
flags=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(s),'-I'+str(w)]
def run(command,name,execution=False):
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1') if execution else None
    result=subprocess.run(command,capture_output=True,text=True,timeout=180,env=env)
    (w/name).write_text(result.stdout+result.stderr)
    if result.returncode:print((result.stdout+result.stderr)[-12000:])
    result.check_returncode();return result.stdout
run(['cc','-std=c11',*flags,'-c',str(s/'Tab5ImageCore.c'),'-o',str(w/'core.o')],'compile-core.log')
run(['g++','-std=c++17',*flags,str(HERE/'viewer.cpp'),str(w/'core.o'),'-o',str(w/'viewer')],'compile-viewer.log')
output=run([str(w/'viewer')],'viewer.log',True);print(output)
assert before=={str(path):sha(path) for path in files},'Source changed during test; repeat after gel'
report={'passed':True,'physicalDeviceTested':False,'method':__doc__,'sourceHashes':before,
    'testSourceHashes':{str(path):sha(path) for path in (Path(__file__),HERE/'viewer.cpp')},
    'extractedHashes':{'viewer-dispatch.inc':sha(w/'viewer-dispatch.inc')},
    'result':json.loads(output.strip().splitlines()[-1]),'sanitizers':['address','undefined'],
    'limits':['Real native T5I parser, layered-image rendering and production signature dispatch; filesystem and canvas allocations use host adapters.',
      'WebP validates only adapter routing and cleanup, with a substitute decoder. JPEG/PNG/BMP decoding and physical SD/device are not tested.',
      'Allocations/reservations checked explicitly after each scenario; LeakSanitizer disabled because host /proc inspection is unavailable.']}
(w/'result.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
