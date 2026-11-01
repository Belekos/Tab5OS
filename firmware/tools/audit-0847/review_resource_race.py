#!/usr/bin/env python3
"""Independent deterministic late-completion test, using actual Arduino manager core."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
sketch = root / 'Tab5OS_Arduino_v0.8.47_Ecriture'
tests = root / 'tools/memory-tests-0847'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--work', required=True, type=Path)
args = parser.parse_args()
args.work.mkdir(parents=True, exist_ok=True)
stubs = args.work / 'stubs'
shutil.copytree(tests / 'stubs', stubs, dirs_exist_ok=True)
events = stubs / 'freertos/event_groups.h'
text = events.read_text().replace(
    'extern std::function<void()> beforeEventWait;',
    'extern std::function<void()> beforeEventWait, beforeEventSet;')
old = 'xEventGroupSetBits(EventGroupHandle_t p,EventBits_t bits){++eventSets;'
assert text.count(old) == 1
text = text.replace(old, old + 'if(beforeEventSet)beforeEventSet();')
events.write_text(text)
source = (sketch / 'Tab5ResourceManager.cpp').read_text()
marker = '\n#ifdef ARDUINO\n#include "Tab5Memory.h"'
assert source.count(marker) == 1
core = args.work / 'core.cpp'
core.write_text(source.split(marker)[0])
binary = args.work / 'late-completion'
command = ['g++', '-std=c++17', '-O1', '-g', '-pthread', '-fno-pie', '-no-pie',
           '-fno-omit-frame-pointer', '-fsanitize=address,undefined', '-Wall', '-Wextra',
           '-Werror', '-Wno-misleading-indentation', '-DARDUINO', '-I' + str(stubs),
           '-I' + str(sketch), str(tests / 'late_completion.cpp'), str(core), '-o', str(binary)]
result = subprocess.run(command, capture_output=True, text=True)
(args.work / 'build.log').write_text(result.stdout + result.stderr)
result.check_returncode()
result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30,
                        env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',
                                 UBSAN_OPTIONS='halt_on_error=1'))
(args.work / 'run.log').write_text(result.stdout + result.stderr)
result.check_returncode()
assert not result.stderr, result.stderr
files = [sketch / 'Tab5ResourceManager.cpp', sketch / 'Tab5ResourceManager.h',
         tests / 'late_completion.cpp', Path(__file__)]
report = {'passed': True, 'result': json.loads(result.stdout),
          'sourceHashes': {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files},
          'scope': 'Exact production manager core in ARDUINO mode. Controlled FreeRTOS boundaries pause an old SetBits until Done is freed and reused by a new Pending request. No actual target scheduling or latency assertion.',
          'command': command, 'physicalDeviceTested': False}
(root / 'tools/verification/resource-race-independent-0847.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
