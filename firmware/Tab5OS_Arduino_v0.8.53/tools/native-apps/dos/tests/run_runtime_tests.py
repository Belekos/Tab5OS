#!/usr/bin/env python3
"""Check the exact DOS runtime memory/division code with guarded pages."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
RUNTIME = HERE.parents[1] / 'common/runtime.c'
REPOSITORY = HERE.parents[5]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path)
args = parser.parse_args()
compiler = os.environ.get('CC', 'cl' if os.name == 'nt' else 'cc')
msvc = Path(compiler).stem.lower() == 'cl'
source = RUNTIME.read_text(encoding='utf-8')


def region(label):
    start = source.index('/* ' + label + '_BEGIN')
    end = source.index('/* ' + label + '_END */', start)
    return source[start:end] + '\n'


with tempfile.TemporaryDirectory(prefix='tab5-dos-runtime-') as temporary:
    work = Path(temporary)
    (work / 'runtime_memory.inc').write_text(region('DOS_MEMORY_PRIMITIVES'), encoding='utf-8')
    (work / 'runtime_division.inc').write_text(region('DOS_DIVISION_PRIMITIVES'), encoding='utf-8')
    binary = work / ('runtime-tests.exe' if os.name == 'nt' else 'runtime-tests')
    if msvc:
        command = [compiler, '/nologo', '/std:c11', '/O2', '/W4', '/WX', '/wd5105',
                   '/D_CRT_SECURE_NO_WARNINGS', '/DTAB5_DOS', '/I' + str(work),
                   str(HERE / 'test_runtime_primitives.c'), '/Fe:' + str(binary)]
    else:
        command = [compiler, '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                   '-Wno-misleading-indentation', '-O2', '-fno-builtin',
                   '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                   '-DTAB5_DOS', '-I' + str(work),
                   str(HERE / 'test_runtime_primitives.c'), '-o', str(binary)]
    subprocess.run(command, cwd=work, check=True)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
    result = subprocess.run([str(binary)], cwd=work, env=env, text=True, capture_output=True, check=True)
    print(result.stdout, end='')
    print(result.stderr, end='')

sources = [RUNTIME, HERE / 'test_runtime_primitives.c', Path(__file__)]
report = {
    'component': 'dos-runtime-primitives', 'passed': True, 'compiler': compiler,
    'sanitizers': [] if msvc else ['AddressSanitizer', 'UndefinedBehaviorSanitizer', 'LeakSanitizer'],
    'sourceHashes': {p.relative_to(REPOSITORY).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
    'coverage': result.stdout.strip(),
    'limitations': ['Exact production function regions run on the host; target timing and PSRAM throughput need hardware measurement.'],
}
if args.output:
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
