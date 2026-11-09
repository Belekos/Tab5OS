#!/usr/bin/env python3
"""Test the production DOS directory adapter and complete FAT folder projection."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
DOS = HERE.parent
FIRMWARE = DOS.parents[2]
REPOSITORY = FIRMWARE.parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path)
args = parser.parse_args()
compiler = os.environ.get('CC', 'cl' if os.name == 'nt' else 'cc')
msvc = Path(compiler).stem.lower() == 'cl'
platform = (DOS / 'platform.c').read_text(encoding='utf-8')
def function(signature):
    start = platform.index(signature)
    cursor = platform.index('{', start)
    depth = 1
    while depth:
        cursor += 1
        if platform[cursor] == '{': depth += 1
        elif platform[cursor] == '}': depth -= 1
    return platform[start:cursor + 1] + '\n'
with tempfile.TemporaryDirectory(prefix='tab5-dos-folder-') as temporary:
    work = Path(temporary)
    signatures = ['DIR *opendir(', 'struct dirent *readdir(', 'int dos_directory_error(', 'int closedir(', 'int stat(']
    (work / 'directory_callbacks.inc').write_text(''.join(map(function, signatures)), encoding='utf-8')
    folder = (DOS / 'core/ibmpc/folder.c').read_text(encoding='utf-8')
    folder = folder.replace('#include <dirent.h>', '').replace('#include <sys/stat.h>', '')
    (work / 'folder_source.inc').write_text(folder, encoding='utf-8')
    output = work / ('folder-tests.exe' if os.name == 'nt' else 'folder-tests')
    include = [FIRMWARE, DOS / 'core/ibmpc', DOS / 'core/x86', work]
    if msvc:
        command = [compiler, '/nologo', '/std:c11', '/W4', '/WX', '/wd4324', '/wd4100', '/D_CRT_SECURE_NO_WARNINGS', '/DTAB5_DOS',
                   '/FI' + str(HERE / 'host-compat/platform.h'), *['/I' + str(p) for p in include],
                   str(HERE / 'test_folder.c'), '/Fe:' + str(output)]
    else:
        command = [compiler, '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation',
                   '-Wno-unused-function', '-Wno-unused-parameter', '-DTAB5_DOS', '-g', '-O1', '-fsanitize=address,undefined',
                   '-fno-omit-frame-pointer', *['-I' + str(p) for p in include],
                   str(HERE / 'test_folder.c'), '-o', str(output)]
    subprocess.run(command, cwd=work, check=True)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
    result = subprocess.run([str(output)], cwd=work, env=env, text=True, capture_output=True, check=True)
    print(result.stdout, end='')
    print(result.stderr, end='')
sources = [DOS / 'platform.c', DOS / 'core/ibmpc/folder.c', DOS / 'core/ibmpc/pc_internal.h',
           DOS / 'core/ibmpc/pc.h', DOS / 'core/x86/x86.h', HERE / 'host-compat/platform.h',
           FIRMWARE / 'Tab5AppApi.h', HERE / 'test_folder.c', Path(__file__)]
report = {'component': 'dos-folder', 'passed': True, 'compiler': compiler,
          'sanitizers': [] if msvc else ['AddressSanitizer', 'UndefinedBehaviorSanitizer', 'LeakSanitizer'],
          'sourceHashes': {p.relative_to(REPOSITORY).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
          'coverage': result.stdout.strip(),
          'limitations': ['Compiles the complete production folder projection and directory callbacks using SDK and stdio doubles; no physical SD or guest CPU is exercised.']}
if args.output:
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
