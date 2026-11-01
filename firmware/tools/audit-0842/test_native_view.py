#!/usr/bin/env python3
"""Host synchronization/failure tests of exact NativeView production functions.

No ESP32 timings are claimed: a frame-copy delay is injected deterministically.
LVGL, admission and capability allocation are mocked; the input queue is real.
"""
import argparse
import hashlib
import json
import pathlib
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--sketch', type=pathlib.Path, required=True)
parser.add_argument('--baseline', action='store_true')
parser.add_argument('--report', type=pathlib.Path)
args = parser.parse_args()
source = (args.sketch / 'Tab5NativeView.cpp').read_text()

def function(signature):
    first = source.index(signature)
    opening = source.index('{', first)
    depth = 1
    pos = opening + 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    return source[first:pos]

parts = [function('SemaphoreHandle_t lock()')]
if 'SemaphoreHandle_t inputLock()' in source:
    parts.append(function('SemaphoreHandle_t inputLock()'))
parts += [function(x) for x in ('void queueInput(', 'bool beginMode(', 'int inputPoll(', 'bool present(', 'void close()')]
parts = [p.replace('memcpy(back,pixels,count*2)', 'copyFrame(back,pixels,count*2)') for p in parts]
tail = source[source.index(' static uint32_t painted='):source.index('\n}\nvoid close()')]
parts.append('void serviceFrame(){\n' + tail + '\n}')
with tempfile.TemporaryDirectory(prefix='tab5-native-view-') as folder:
    folder = pathlib.Path(folder)
    (folder / 'production.inc').write_text('\n'.join(parts))
    binary = folder / 'test'
    subprocess.run(['g++', '-std=c++17', '-pthread', '-O1', '-g', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', '-I', str(folder), '-I', str(args.sketch),
                    str(pathlib.Path(__file__).with_suffix('.cpp')), '-o', str(binary)], check=True)
    run = subprocess.run([str(binary), 'baseline' if args.baseline else 'fixed'], check=True, capture_output=True, text=True)
    print(run.stdout, end='')
    if args.report:
        paths = [args.sketch / name for name in ['Tab5NativeView.cpp', 'Tab5NativeView.h', 'Tab5AppInputQueue.h', 'Tab5AppApi.h']]
        paths += [pathlib.Path(__file__), pathlib.Path(__file__).with_suffix('.cpp')]
        args.report.write_text(json.dumps({'passed': True, 'physicalDeviceTested': False,
            'baseline': args.baseline, 'output': run.stdout.strip(),
            'scope': 'Exact production frame/input/lifetime functions; real input queue; mocked LVGL, admission and heap capabilities. Injected host contention is not ESP32 timing. ASan/UBSan; capability allocations counted explicitly; LSan unavailable under executor ptrace.',
            'sourceHashes': {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}}, indent=2) + '\n')
