#!/usr/bin/env python3
"""Run the production Doom input/timing adapter against host service stubs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
DOOM = HERE.parent
REPO = DOOM.parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path)
args = parser.parse_args()
compiler = os.environ.get('CC', 'cc')
msvc = Path(compiler).name.lower() in ('cl', 'cl.exe')
with tempfile.TemporaryDirectory(prefix='tab5-doom-input-') as work:
    binary = Path(work) / ('test-input.exe' if os.name == 'nt' else 'test-input')
    # Keep the real service ABI while using the host C library, not ELF wrappers.
    host_include = Path(work) / 'include'
    host_include.mkdir()
    shutil.copy2(DOOM / 'compat/psram_app.h', host_include / 'psram_app.h')
    if msvc:
        flags = ['/nologo', '/std:c11', '/W3', '/Od', '/D_CRT_SECURE_NO_WARNINGS', '/DPAPP_APP_SIDE=1',
                 '/I' + str(host_include), '/I' + str(DOOM / 'core')]
        command = [compiler, *flags, str(HERE / 'test_input.c'), '/Fe:' + str(binary)]
    else:
        flags = ['-std=gnu11', '-Wall', '-Wextra', '-Wno-unused-parameter', '-DPAPP_APP_SIDE=1', '-g',
                 '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                 '-I', str(host_include), '-I', str(DOOM / 'core')]
        command = [compiler, *flags, str(HERE / 'test_input.c'), '-o', str(binary)]
    subprocess.run(command, check=True, cwd=work)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    result = subprocess.run([str(binary)], check=True, capture_output=True, text=True, env=env)
    print(result.stdout, end='')
    if result.stderr:
        print(result.stderr, end='')
    report = json.loads(result.stdout.strip().splitlines()[-1])
report.update({
    'component': 'Doom input and clock', 'compiler': compiler, 'flags': flags,
    'sanitizers': [] if msvc else ['AddressSanitizer', 'UndefinedBehaviorSanitizer'],
    'sourceHashes': {str(p.relative_to(REPO)).replace('\\', '/'): hashlib.sha256(p.read_bytes()).hexdigest()
                     for p in (DOOM / 'doom_input.h', DOOM / 'papp_i_system.c', HERE / 'test_input.c', Path(__file__))},
    'coverage': ['title and attract-demo entry', 'menu confirmation and back',
                 'gameplay mappings and held/released keys', 'shared-key ownership',
                 'context transitions and remapped keys', 'startup input',
                 'Ctrl+S with delayed OS requests', 'clock overflow and millis wrap'],
    'limitations': ['Host service stubs do not exercise physical USB/touch devices, the complete Doom engine or ESP32 ELF loading.'],
})
if args.output:
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(args.output)
