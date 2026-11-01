#!/usr/bin/env python3
"""Execute pinned M5 mixer and end function; native I2S/scheduling are controlled."""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path

def block(text,prefix):
 start=text.index(prefix);depth=0
 for end in range(text.index('{',start),len(text)):
  if text[end]=='{':depth+=1
  elif text[end]=='}':
   depth-=1
   if depth==0:return text[start:end+1]
 raise ValueError(prefix)
def digest(data):return hashlib.sha256(data).hexdigest()
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sdk',required=True,type=Path);p.add_argument('--baseline-helper',required=True,type=Path);p.add_argument('--work',required=True,type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;root=here.parents[1];a.work.mkdir(parents=True,exist_ok=True)
results={};hashes={}
for label,helper in [('before',a.baseline_helper),('after',root/'tools/build_linux/prepare_m5unified_audio.py')]:
 spec=importlib.util.spec_from_file_location(label,helper);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);patched=module.patched_files(a.sdk)
 speaker=patched['src/utility/Speaker_Class.inl'].decode();header=patched['src/utility/Speaker_Class.hpp'].decode()
 if label=='before':assert digest(patched['src/utility/Speaker_Class.inl'])=='c8c4a28ae068c7f953179321a2fe9e4413dfea78803c38ceb4b0429bb29bfa21'
 types=header[header.index('    struct wav_info_t'):header.index('    channel_info_t _ch_info')]
 functions='\n\n'.join(block(speaker,prefix) for prefix in ['  void Speaker_Class::wav_info_t::clear(', '  void Speaker_Class::spk_task(', '  void Speaker_Class::_end_locked('])
 fixture=(here/'mixer_fixture.cpp').read_text().replace('// CONFIG',block(header,'  struct speaker_config_t')+';').replace('// CHANNEL_TYPES',types).replace('// FUNCTIONS',functions)
 source=a.work/(label+'.cpp');source.write_text(fixture);binary=a.work/label
 c=['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-DPATCHED='+str(int(label=='after')),str(source),'-o',str(binary)]
 r=subprocess.run(c,capture_output=True,text=True);(a.work/(label+'-build.log')).write_text(r.stdout+r.stderr)
 if r.returncode:print(r.stderr)
 r.check_returncode()
 r=subprocess.run([str(binary),str(a.work/(label+'.pcm'))],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));(a.work/(label+'-run.log')).write_text(r.stdout+r.stderr);print(label,r.stdout,r.stderr);r.check_returncode();results[label]=json.loads(r.stdout)
 hashes[label]={'helperSha256':digest(helper.read_bytes()),'speakerSha256':digest(patched['src/utility/Speaker_Class.inl'])}
assert (a.work/'before.pcm').read_bytes()==(a.work/'after.pcm').read_bytes()
report={'passed':True,'version':'0.8.48',**results,'continuousPcmIdentical':True,'comparativeSourceHashes':hashes,'sourceHashes':{str(q.relative_to(root)):digest(q.read_bytes()) for q in [root/'tools/build_linux/prepare_m5unified_audio.py',Path(__file__),here/'mixer_fixture.cpp']},'fixtureSha256':digest((here/'mixer_fixture.cpp').read_bytes()),'runnerSha256':digest(Path(__file__).read_bytes()),'scope':'Actual full spk_task, channel definitions and _end_locked from pinned M5Unified 0.2.23, before is exact .47 patch hash. Native I2S output and task scheduling controlled; restart stops at a DMA boundary. No analogue pop or hardware timing guarantee. ASan/UBSan enabled; LSan unavailable under this sandbox (proc task access denied).'}
(root/'tools/verification/audio-reset-0848.json').write_text(json.dumps(report,indent=2)+'\n')
