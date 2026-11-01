#!/usr/bin/env python3
"""Check exact production lifecycle methods with controlled worker/I/O boundaries."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
root=Path(__file__).resolve().parents[2];sketch=Path(os.environ.get('TAB5_TEST_SKETCH',root/'Tab5OS_Arduino_v0.8.49_Ecriture'));here=Path(__file__).resolve().parent
source=(sketch/'Tab5Video.cpp').read_text()
def block(text,start):
 pos=text.index(start);brace=text.index('{',pos);depth=0
 for end in range(brace,len(text)):
  if text[end]=='{':depth+=1
  elif text[end]=='}':
   depth-=1
   if not depth:return text[pos:end+1]
 raise AssertionError(start)
names=['bool Tab5VideoEngine::ensureJpegInput(', 'bool Tab5VideoEngine::ensureDecoderBuffers(', 'void Tab5VideoEngine::releaseFrames(', 'void Tab5VideoEngine::releaseDecoderResources(', 'void Tab5VideoEngine::setState(', 'bool Tab5VideoEngine::decodeAndPublish(']
names += ['bool Tab5VideoEngine::playSource(', 'void Tab5VideoEngine::preparePlaybackLocked(', 'void Tab5VideoEngine::stop(', 'void Tab5VideoEngine::service(', 'void Tab5VideoEngine::detachDisplay(', 'bool Tab5VideoEngine::frame(', 'void Tab5VideoEngine::taskEntry(']
if 'void Tab5VideoEngine::trimIdleFrames(' in source:names.append('void Tab5VideoEngine::trimIdleFrames(')
methods='\n\n'.join(block(source,n) for n in names)
cleanup=block(source,'void Tab5VideoEngine::taskMain(');cleanup=cleanup[cleanup.index('  tab5DiagnosticMark(4,"video-release"'):].rsplit('}',1)[0]
fixture=(here/'fixture.cpp').read_text().replace('// NATIVE_ALLOCATOR',block((root/'tools/video-tests-0848/sdk/jpeg_decode.c').read_text(),'void *jpeg_alloc_decoder_mem(')).replace('// PRODUCTION_GUARD',block(source,'class MutexGuard')+';').replace('// PRODUCTION_METHODS',methods).replace('// PRODUCTION_CLEANUP',cleanup)
build=Path(tempfile.mkdtemp(prefix='tab5-video-buffers-'))
for n in ['Arduino.h','freertos/FreeRTOS.h','freertos/semphr.h','freertos/task.h']:
 p=build/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#pragma once\n')
(build/'Tab5JpegHeader.h').write_bytes((sketch/'Tab5JpegHeader.h').read_bytes());(build/'Tab5CatalogueRevision.h').write_bytes((sketch/'Tab5CatalogueRevision.h').read_bytes());(build/'Tab5Video.h').write_bytes((sketch/'Tab5Video.h').read_bytes());(build/'test.cpp').write_text(fixture)
cmd=['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-pthread','-fno-pie','-no-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined','-I'+str(build),str(build/'test.cpp'),'-o',str(build/'test')]
subprocess.run(cmd,check=True)
run=subprocess.run([str(build/'test')],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'));print(run.stdout+run.stderr,end='');run.check_returncode()
report={'version':'0.8.49',**json.loads(run.stdout.strip()),'scope':'Exact production playSource/taskEntry/service/frame/detachDisplay/stop and decoder allocation/publication methods, class declaration and taskMain cleanup tail. Input task body uses a controlled JPEG at the parser boundary; actual bounded JPEG header probe. SDK DMA decoder and heap boundaries controlled; uninitialized outputs poisoned with ASan. No physical decoder/network/LVGL execution. ASan+UBSan, explicit allocation tracker; LSan unavailable in this runtime.','sourceHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [sketch/'Tab5Video.cpp',sketch/'Tab5Video.h',sketch/'Tab5JpegHeader.h',here/'run.py',here/'fixture.cpp',root/'tools/video-tests-0848/sdk/jpeg_decode.c']}}
out=root/'tools/verification/video-lifecycle-0849.json';out.write_text(json.dumps(report,indent=2)+'\n');print(out)
