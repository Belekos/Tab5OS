#!/usr/bin/env python3
"""Run real Tab5AudioDriver + prepared M5 Speaker lifecycle/worker on controlled RTOS/I2S."""
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
def stripincludes(s):return '\n'.join(l for l in s.splitlines() if not l.startswith('#include') and l!='#pragma once')
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sdk',required=True,type=Path);p.add_argument('--work',required=True,type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;root=here.parents[1];sketch=root/'Tab5OS_Arduino_v0.8.49_Ecriture';a.work.mkdir(parents=True,exist_ok=True)
helper=root/'tools/build_linux/prepare_m5unified_audio.py';spec=importlib.util.spec_from_file_location('patch',helper);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);patched=m.patched_files(a.sdk)
speaker=patched['src/utility/Speaker_Class.inl'].decode();header=patched['src/utility/Speaker_Class.hpp'].decode()
functions='\n\n'.join(block(speaker,prefix) for prefix in ['  void Speaker_Class::wav_info_t::clear(', '  void Speaker_Class::spk_task(', '  bool Speaker_Class::begin(', '  bool Speaker_Class::_begin_raw(', '  void Speaker_Class::end(', '  void Speaker_Class::_end_locked(', '  void Speaker_Class::stop(void)', '  void Speaker_Class::stop(uint8_t', '  bool Speaker_Class::stopAndWait(', '  bool Speaker_Class::_set_next_wav(', '  int Speaker_Class::_set_next_wav_locked(', '  void Speaker_Class::_lock_req(', '  bool Speaker_Class::_in_task(', '  bool Speaker_Class::_play_raw('])
fixture=(here/'fixture.cpp').read_text().replace('// SPEAKER_CONFIG',block(header,'  struct speaker_config_t')+';').replace('// SPEAKER_CLASS',block(header,'  class Speaker_Class\n').replace('  friend M5Unified;','').replace('  protected:','  public:')+';').replace('// SPEAKER_FUNCTIONS',functions).replace('// DRIVER_HEADER',stripincludes((sketch/'Tab5AudioDriver.h').read_text()).replace(' private:',' public:')).replace('// DRIVER_SOURCE',stripincludes((sketch/'Tab5AudioDriver.cpp').read_text()))
audio=(sketch/'Tab5Audio.cpp').read_text()
engine_functions='\n\n'.join(block(audio,prefix) for prefix in ['void Tab5AudioEngine::cleanupDecoder(', 'void Tab5AudioEngine::stopPlayback(', 'void Tab5AudioEngine::serviceMemoryPriority('])
flac=block(audio,'class Tab5SeekableFlacGenerator')
flac_begin=block(flac,'  bool begin(');flac_stop=block(flac,'  bool stop(')
fixture=fixture.replace('// PRIORITY_HEADER',stripincludes((sketch/'Tab5AudioPriority.h').read_text()))
fixture=fixture.replace('// ENGINE_FUNCTIONS',engine_functions).replace('// FLAC_BEGIN',flac_begin).replace('// FLAC_STOP',flac_stop)
(a.work/'driver').mkdir(exist_ok=True);(a.work/'driver/i2s_std.h').write_text('')
source=a.work/'integrated.cpp';source.write_text(fixture);binary=a.work/'integrated'
command=['g++','-std=c++17','-O1','-g','-pthread','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-but-set-variable','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-I'+str(a.work),str(source),'-o',str(binary)]
r=subprocess.run(command,capture_output=True,text=True);(a.work/'build.log').write_text(r.stdout+r.stderr);print(r.stderr);r.check_returncode()
r=subprocess.run([str(binary)],capture_output=True,text=True,timeout=90,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));(a.work/'run.log').write_text(r.stdout+r.stderr);print(r.stdout,r.stderr);r.check_returncode();data=json.loads(r.stdout)
report={'passed':True,**data,'scope':'Actual complete Tab5AudioDriver/AudioOutput and M5Unified channel definitions, request publication, stopAndWait, begin/end and full spk_task. RTOS semaphore/task and I2S hardware boundaries controlled by host threads. ASan/UBSan; no physical audio or ESP32 scheduler timing guarantee.','sourceHashes':{str(q.relative_to(root)):hashlib.sha256(q.read_bytes()).hexdigest() for q in [helper,sketch/'Tab5AudioDriver.cpp',sketch/'Tab5AudioDriver.h',sketch/'Tab5Audio.cpp',sketch/'Tab5Audio.h',sketch/'Tab5AudioPriority.h',Path(__file__),here/'fixture.cpp']},'speakerSha256':hashlib.sha256(patched['src/utility/Speaker_Class.inl']).hexdigest()}
(root/'tools/audit-0849/audio-contract-tests.json').write_text(json.dumps(report,indent=2)+'\n')
