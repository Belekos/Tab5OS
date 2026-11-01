#!/usr/bin/env python3
"""Execute real M5 startup/teardown/codec functions against controlled I2S/I2C."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import os
import subprocess


def block(text, prefix):
    start = text.index(prefix)
    brace = text.index('{', start)
    depth = 0
    for end in range(brace, len(text)):
        if text[end] == '{':
            depth += 1
        elif text[end] == '}':
            depth -= 1
            if depth == 0:
                return text[start:end+1]
    raise ValueError(prefix)


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdk', required=True, type=Path)
parser.add_argument('--work', required=True, type=Path)
args = parser.parse_args()
here = Path(__file__).resolve().parent
root = here.parents[1]
helper = root/'tools/build_linux/prepare_m5unified_audio.py'
spec = importlib.util.spec_from_file_location('prepare_audio', helper)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
patched = module.patched_files(args.sdk)
args.work.mkdir(parents=True, exist_ok=True)
(args.work/'driver').mkdir(exist_ok=True)
(args.work/'driver/i2s_std.h').write_text('// Availability marker; native I2S boundary is in the fixture.\n')
results = {}
for label in ['before', 'after']:
    texts = {name: (data.decode() if label == 'after' else (args.sdk/name).read_text())
             for name, data in patched.items()}
    speaker = texts['src/utility/Speaker_Class.inl']
    unified = texts['src/M5Unified.inl']
    config = block(texts['src/utility/Speaker_Class.hpp'], '  struct speaker_config_t')+';'
    if label == 'before':
        config = config[:-2] + 'bool enable_after_i2s = false;\n};'
    fixture = (here/'startup_fixture.cpp').read_text()
    fixture = fixture.replace('// CONFIGURATION', config)
    fixture = fixture.replace('// BULK_WRITE', block(unified, '  static bool in_i2c_bulk_write('))
    fixture = fixture.replace('// CODEC_CALLBACK', block(unified, '  bool M5Unified::_speaker_enabled_cb_tab5('))
    functions = '\n\n'.join(block(speaker, prefix) for prefix in [
        '  bool Speaker_Class::begin(', '  bool Speaker_Class::_begin_raw(',
        '  void Speaker_Class::end(', '  void Speaker_Class::_end_locked('])
    fixture = fixture.replace('// SPEAKER_METHODS', functions)
    source = args.work/(label+'.cpp')
    source.write_text(fixture)
    binary = args.work/label
    command = ['g++', '-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
               '-Wno-unused-parameter', '-Wno-unused-variable', '-fno-omit-frame-pointer',
               '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
               '-I'+str(args.work), '-DPATCHED='+('1' if label == 'after' else '0'), str(source), '-o', str(binary)]
    process = subprocess.run(command, capture_output=True, text=True)
    (args.work/(label+'-build.log')).write_text(process.stdout+process.stderr)
    if process.returncode:
        print(process.stderr)
    process.check_returncode()
    process = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30,
                             env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',
                                      UBSAN_OPTIONS='halt_on_error=1'))
    (args.work/(label+'-run.log')).write_text(process.stdout+process.stderr)
    print(label, process.stdout, process.stderr)
    process.check_returncode()
    results[label] = json.loads(process.stdout)

speaker = patched['src/utility/Speaker_Class.inl'].decode()
idle = block(speaker, '      if (flg_nodata)')
first = block(speaker, '      if (flg_i2s_started != spk_i2s_run)')
fixture = (here/'idle_fixture.cpp').read_text().replace('// ACTUAL_IDLE',idle).replace('// ACTUAL_FIRST_WORK',first)
source=args.work/'idle.cpp';source.write_text(fixture);binary=args.work/'idle'
process=subprocess.run(['g++','-std=c++17','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(source),'-o',str(binary)],capture_output=True,text=True)
(args.work/'idle-build.log').write_text(process.stdout+process.stderr)
if process.returncode: print(process.stderr)
process.check_returncode()
process=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
(args.work/'idle-run.log').write_text(process.stdout+process.stderr);print(process.stdout+process.stderr);process.check_returncode()
results['idle']=json.loads(process.stdout)
report = {'version':'0.8.48', 'passed':True, **results,
          'sourceHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [helper,Path(__file__),here/'startup_fixture.cpp',here/'idle_fixture.cpp']},
          'upstreamHashes':module.EXPECTED,
          'patchedHashes':{name:module.digest(value) for name,value in patched.items()},
          'testHashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()
                        for p in [Path(__file__), here/'startup_fixture.cpp', here/'idle_fixture.cpp', helper]},
          'scope':'Actual M5 begin/_begin_raw/end/_end_locked and Tab5 codec callback plus bulk I2C helper. I2S DMA/FreeRTOS task scheduling/I2C are controlled boundaries. Checks byte-zero priming, ordering, every startup I2C failure, partial writes, setup/start/task failures and non-opt-in path. The actual spk_task idle and first-work branches also run for 1000 external-codec cycles and an internal-DAC control. No acoustic or physical timing claim. ASan/UBSan; LSan disabled.'}
(root/'tools/verification/audio-startup-regression-0848.json').write_text(json.dumps(report, indent=2)+'\n')
