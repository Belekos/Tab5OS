#!/usr/bin/env python3
"""Exercise exact production input/worker and the locked SDK ownership methods."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,tempfile

here=Path(__file__).resolve().parent;root=here.parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--sdk',type=Path,required=True)
parser.add_argument('--baseline',type=Path,default=here/'baseline_0843.txt');args=parser.parse_args()
sketch=root/'Tab5OS_Arduino_v0.8.44_Ecriture';source=(sketch/'Tab5Video.cpp').read_text()
sdk=args.sdk.read_text()
def block(text,start):
 pos=text.index(start);brace=text.index('{',pos);depth=0
 for end in range(brace,len(text)):
  if text[end]=='{':depth+=1
  elif text[end]=='}':
   depth-=1
   if not depth:return text[pos:end+1]
 raise AssertionError(start)
sdk_methods='\n'.join(block(sdk,n) for n in ['HTTPClient::~HTTPClient()',
 'void HTTPClient::end(', 'void HTTPClient::disconnect(', 'bool HTTPClient::connected(', 'NetworkClient *HTTPClient::getStreamPtr('])
def test(name,src,baseline=False):
 build=Path(tempfile.mkdtemp(prefix='tab5-video-input-'+name+'-'))
 for f in ['Arduino.h','freertos/FreeRTOS.h','freertos/semphr.h','freertos/task.h']:
  p=build/f;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#pragma once\n')
 (build/'Tab5Video.h').write_bytes((sketch/'Tab5Video.h').read_bytes())
 fixture=(here/'input_fixture.cpp').read_text().replace('// SDK_HTTP_METHODS',sdk_methods)
 fixture=fixture.replace('// PRODUCTION_INPUT',block(src,'class Tab5VideoInput')+';')
 helpers=['uint32_t le32(', 'uint16_t le16(', 'bool fourcc(',
  'struct AviInfo', 'struct AviStreamInfo', 'bool skipChunk(', 'bool readChunkHeader(',
  'bool readSmallChunk(', 'bool parseStrl(', 'bool parseHdrl(', 'int streamNumber(']
 fixture=fixture.replace('// PRODUCTION_AVI_HELPERS','\n'.join(block(src,n)+(';' if n.startswith('struct') else '') for n in helpers))
 fixture=fixture.replace('// PRODUCTION_AVI',block(src,'bool Tab5VideoEngine::playAvi('))
 fixture=fixture.replace('// PRODUCTION_GUARD',block(src,'class MutexGuard')+';')
 fixture=fixture.replace('// PRODUCTION_TASK',block(src,'void Tab5VideoEngine::taskMain('))
 fixture=fixture.replace(' INPUT_PRIORITY',',priority' if baseline else '')
 (build/'test.cpp').write_text(fixture)
 subprocess.run(['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror',
  '-fno-pie','-no-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined',
  '-I'+str(build),str(build/'test.cpp'),'-o',str(build/'test')],check=True)
 result=subprocess.run([str(build/'test')],capture_output=True,text=True,timeout=30,
  env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
 if baseline:
  assert result.returncode!=0 and 'heap-use-after-free' in result.stderr,result.stdout+result.stderr
  (root/'tools/audit-0844/video-input-before-asan.log').write_text(result.stderr)
  return {'heapUseAfterFreeReproduced':True,'exitCode':result.returncode}
 print(result.stdout+result.stderr,end='');result.check_returncode()
 return json.loads(result.stdout.strip())
before=test('baseline',args.baseline.read_text(),True);after=test('fixed',source)
report={'version':'0.8.44','passed':True,'physicalDeviceTested':False,
 'baseline':before,'after':after,
 'scope':'Exact Tab5VideoInput class and taskMain, actual class declaration and AVI parser/playAvi, exact locked SDK HTTPClient destructor/end/disconnect/connected/getStreamPtr; network, FreeRTOS and decoder boundaries controlled. ASan+UBSan; no physical HTTP/TLS/JPEG run.',
 'sourceHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [sketch/'Tab5Video.cpp',sketch/'Tab5Video.h',here/'input.py',here/'input_fixture.cpp']},
 'baselineSha256':hashlib.sha256(args.baseline.read_bytes()).hexdigest(),
 'sdkHttpClientSha256':hashlib.sha256(args.sdk.read_bytes()).hexdigest()}
out=root/'tools/audit-0844/video-input.json';out.write_text(json.dumps(report,indent=2)+'\n');print(out)
