import hashlib,json,pathlib,re,subprocess,tempfile,os
root=pathlib.Path(__file__).resolve().parents[2];sketch=root/'Tab5OS_Arduino_v0.8.39_Ecriture';tests=pathlib.Path(__file__).resolve().parent
out=root/'tools/verification/usb-ipp-0839';out.mkdir(parents=True,exist_ok=True)
sources=['Tab5UsbPrinter.h','Tab5UsbPrinter.cpp','Tab5UsbIppProtocol.h','WriterUsb.cpp','Tab5Print.cpp','Tab5PrinterDiscovery.cpp']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
hashes={str((sketch/p).relative_to(root)):sha(sketch/p) for p in sources}
checks=0
with tempfile.TemporaryDirectory(prefix='tab5-usb-ipp-') as tmp:
 t=pathlib.Path(tmp);(t/'printer.cpp').write_text('#include "Platform.h"\n'+re.sub(r'^#include .*\n','',(sketch/'Tab5UsbPrinter.cpp').read_text(),flags=re.M))
 env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1'
 for test in ['protocol','transport']:
  args=['g++','-std=c++17','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(t),'-I'+str(tests),'-I'+str(sketch),str(tests/(test+'_test.cpp')),'-o',str(t/test)]
  p=subprocess.run(args,capture_output=True,text=True);(out/(test+'-build.log')).write_text(p.stdout+p.stderr);p.check_returncode()
  p=subprocess.run([str(t/test)],capture_output=True,text=True,env=env);(out/(test+'-run.log')).write_text(p.stdout+p.stderr);p.check_returncode();checks+=json.loads(p.stdout)['checks']
report={'version':'0.8.39','passed':True,'checks':checks,'physicalDeviceTested':False,'sourceStableDuringTests':all(sha(root/p)==h for p,h in hashes.items()),'sourceHashes':hashes,'testSourceHashes':{str(p.relative_to(root)):sha(p) for p in tests.iterdir() if p.is_file()},'evidenceHashes':{str(p.relative_to(root)):sha(p) for p in out.iterdir() if p.is_file()},'method':'Production protocol header plus entire production USB printer driver with include substitution and deterministic USB/FreeRTOS/memory boundaries. HTTP Content-Length/chunked framing, fragmented/partial transfers, stale device, disconnect, cancellation, failed admission, no replay after poisoned channel. Native ASan/UBSan; LSan unavailable under this runner. Target source syntax also checked separately with pinned P4 toolchain.','limitations':['No physical USB printer tested. Only IPP-over-USB class 7/1/4 is supported; legacy USB languages need other drivers.','Transport cancellation or incomplete HTTP response requires reconnecting the printer; no job is automatically resent.','Existing printer requires PWG Raster color supported by queried IPP capabilities.']}
(out.parent/'usb-ipp-0839.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['passed','checks','sourceStableDuringTests']}))
