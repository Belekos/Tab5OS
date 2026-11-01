import hashlib,json,pathlib,re,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[2]
sketch=root/'Tab5OS_Arduino_v0.8.39_Ecriture'
tests=pathlib.Path(__file__).resolve().parent
out=root/'tools/verification/usb-format-0839';out.mkdir(parents=True,exist_ok=True)
sources=['Tab5UsbStorage.cpp','Tab5UsbStorage.h','Tab5UsbFormatPolicy.h','src/usb_msc/tab5_msc_format.c','src/usb_msc/tab5_msc_format.h']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
hashes={str((sketch/p).relative_to(root)):sha(sketch/p) for p in sources}
with tempfile.TemporaryDirectory(prefix='tab5-usb-format-') as tmp:
 t=pathlib.Path(tmp)
 source=(sketch/'Tab5UsbStorage.cpp').read_text()
 (t/'controller.cpp').write_text('#include "Platform.h"\n'+re.sub(r'^#include .*\n','',source,flags=re.M))
 cmd=['g++','-std=c++17','-Wall','-Wextra','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(t),'-I'+str(tests),'-I'+str(sketch),str(tests/'controller_test.cpp'),'-o',str(t/'controller')]
 build=subprocess.run(cmd,capture_output=True,text=True);(out/'controller-build.log').write_text(build.stdout+build.stderr);build.check_returncode()
 env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1'
 result=subprocess.run([str(t/'controller')],capture_output=True,text=True,env=env);(out/'controller-run.log').write_text(result.stdout+result.stderr);result.check_returncode()
 result=json.loads(result.stdout)
 (t/'raw.c').write_text(re.sub(r'^#include .*\n','',(sketch/'src/usb_msc/tab5_msc_format.c').read_text(),flags=re.M))
 cmd=['gcc','-std=c11','-Wall','-Wextra','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(t),str(tests/'raw_test.c'),'-o',str(t/'raw')]
 build=subprocess.run(cmd,capture_output=True,text=True);(out/'raw-build.log').write_text(build.stdout+build.stderr);build.check_returncode()
 raw=subprocess.run([str(t/'raw')],capture_output=True,text=True,env=env);(out/'raw-run.log').write_text(raw.stdout+raw.stderr);raw.check_returncode()
 result['checks']+=json.loads(raw.stdout)['checks']
 report={'version':'0.8.39','passed':result['passed'],'checks':result['checks'],'physicalDeviceTested':False,'sourceStableDuringTests':all(sha(root/p)==h for p,h in hashes.items()),'sourceHashes':hashes,'testSourceHashes':{str(p.relative_to(root)):sha(p) for p in tests.iterdir() if p.is_file()},'evidenceHashes':{str(p.relative_to(root)):sha(p) for p in out.iterdir() if p.is_file()},'method':'Whole production USB storage controller and raw formatting routine compiled with only include substitution; deterministic MSC/FreeRTOS/FatFS boundary doubles, native ASan/UBSan. LSan unavailable under this runner. Does not emulate actual USB or FatFs filesystem creation. The raw assertion count includes per-byte erasure checks.','limitations':['No physical storage has been formatted. Hardware filesystem creation and USB disconnect transport remain to validate on Tab5.']}
 (out.parent/'usb-format-0839.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['passed','checks','sourceStableDuringTests']}))
