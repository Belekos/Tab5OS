#!/usr/bin/env python3
"""Compile the keyboard/scope helpers and actual directory broker callbacks."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
FIRMWARE = HERE.parents[2]
REPOSITORY = FIRMWARE.parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path)
args = parser.parse_args()
compiler = os.environ.get('CXX', 'cl' if os.name == 'nt' else 'c++')
msvc = Path(compiler).stem.lower() == 'cl'
source_path = FIRMWARE / 'Tab5Executable.cpp'
# This source historically has CRCRLF lines. Normalize only for extraction.
source = source_path.read_bytes().replace(b'\r', b'').decode('utf-8')
def extract(signature, source=source):
    start = source.index(signature)
    cursor = source.index('{', start)
    depth = 1
    while depth:
        cursor += 1
        if source[cursor] == '{':
            depth += 1
        elif source[cursor] == '}':
            depth -= 1
    return source[start:cursor + 1] + '\n'
signatures = ['bool validPath(', 'int appOpen(', 'File* appFile(', 'int appRead(',
              'int appSeek(', 'int appClose(', 'int appDirectoryOpen(', 'int appDirectoryNext(']
results = []
with tempfile.TemporaryDirectory(prefix='tab5-abi7-') as temp:
    work = Path(temp)
    (work / 'broker_callbacks.inc').write_text(''.join(map(extract, signatures)), encoding='utf-8')
    native = (FIRMWARE / 'Tab5NativeView.cpp').read_bytes().replace(b'\r', b'').decode('utf-8')
    usb = (FIRMWARE / 'WriterUsb.cpp').read_bytes().replace(b'\r', b'').decode('utf-8')
    (work / 'native_keyboard.inc').write_text(''.join(extract(s, native) for s in
        ['void keyboardScreenEvent(', 'int keyboardState(', 'void key(']), encoding='utf-8')
    (work / 'usb_snapshot.inc').write_text(extract('bool usbKeyboardState(', usb), encoding='utf-8')
    for name in ['test_keyboard_directory', 'test_broker', 'test_native_keyboard']:
        executable = work / (name + ('.exe' if os.name == 'nt' else ''))
        if msvc:
            command = [compiler, '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX',
                       '/D_CRT_SECURE_NO_WARNINGS', '/I' + str(FIRMWARE), '/I' + str(work),
                       str(HERE / (name + '.cpp')), '/Fe:' + str(executable)]
        else:
            command = [compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                       '-Wno-misleading-indentation', '-g', '-O1', '-fsanitize=address,undefined',
                       '-fno-omit-frame-pointer', '-pthread', '-I' + str(FIRMWARE), '-I' + str(work),
                       str(HERE / (name + '.cpp')), '-o', str(executable)]
        subprocess.run(command, cwd=work, check=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
        result = subprocess.run([str(executable)], cwd=work, env=env, text=True, capture_output=True, check=True)
        print(result.stdout, end='')
        results.append({'test': name, 'result': 'passed', 'output': result.stdout.strip()})
sources = [source_path, FIRMWARE / 'Tab5NativeView.cpp', FIRMWARE / 'WriterUsb.cpp', FIRMWARE / 'Tab5AppApi.h', FIRMWARE / 'Tab5AppDirectory.h',
           FIRMWARE / 'Tab5KeyboardState.h', FIRMWARE / 'WriterKeys.h', FIRMWARE / 'Tab5AppInputQueue.h', *sorted(HERE.glob('*.*'))]
report = {'component': 'native-abi7', 'passed': True, 'compiler': compiler, 'tests': results,
          'sanitizers': [] if msvc else ['AddressSanitizer', 'UndefinedBehaviorSanitizer'],
          'sourceHashes': {p.relative_to(REPOSITORY).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
          'limitations': ['Host tests compile production callbacks with File/storage and LVGL event doubles. A host mutex replaces the USB portMUX critical section.',
                          'These tests do not exercise physical USB/SD devices, ESP32 scheduling, actual LVGL screen transitions or ELF loading.']}
if args.output:
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
