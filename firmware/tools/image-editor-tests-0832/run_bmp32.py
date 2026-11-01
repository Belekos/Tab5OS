#!/usr/bin/env python3
"""Independent BMP32 BI_RGB fixtures against the frozen production image core."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
here=Path(__file__).resolve().parent
release=here.parents[1]
core=release/'Tab5OS_Arduino_v0.8.32_Ecriture/tools/native-apps/image-editor'
files=[here/'bmp32.c',here/'run_bmp32.py',core/'image_core.c',core/'image_core.h']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
before={str(p.relative_to(release)):sha(p) for p in files}
flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer']
with tempfile.TemporaryDirectory(prefix='tab5-image-bmp32-') as work:
 binary=Path(work)/'bmp32'
 subprocess.run(['cc',*flags,'-I',str(core),str(here/'bmp32.c'),str(core/'image_core.c'),'-o',str(binary)],check=True)
 env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
 result=subprocess.run([str(binary)],env=env,check=True,text=True,capture_output=True)
 summary=json.loads(result.stdout);print(result.stdout,end='')
 after={str(p.relative_to(release)):sha(p) for p in files};assert before==after
 report={'version':'0.8.32','passed':True,'component':'image-editor-bmp32-import','sourceStableDuringTests':True,'sourceHashes':after,'results':summary,'sanitizers':['AddressSanitizer','UndefinedBehaviorSanitizer'],'flags':flags,'coverage':['Production ie_load_bmp, not a duplicate decoder','Two2x2 uncompressed32-bit BI_RGB fixtures with positive(bottom-up) and negative(top-down) height','All four reserved fourth-byte values0/64/128/255 ignored; output pixels opaque, exact RGB/order checked','Every truncation length0..69 for bothfixtures leaves existing image unchanged','19 malformed headers: signature,DIBsize,offset,width,height includingINT32_MIN,planes,depth,compression','Three-byte partialstream reads, <=4096callbackbounds, zero trackedallocatedbytes afterdestruction'],'limitations':['Host tests do not exercise physicalSD or display. LeakSanitizer disabled for sandbox/proc limitation; independentallocator checks zero livebytes.']}
 out=release/'tools/verification/image-editor-bmp32-0832.json';out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(out)
