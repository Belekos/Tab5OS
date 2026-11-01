#!/usr/bin/env python3
"""Compile exact production ELF lifecycle/loader functions with SDK boundaries mocked."""
import argparse
import hashlib
import json
import pathlib
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--sketch', type=pathlib.Path, required=True)
p.add_argument('--loader', action='store_true')
p.add_argument('--report', type=pathlib.Path)
args = p.parse_args()
source = (args.sketch / 'Tab5Executable.cpp').read_bytes().decode().replace('\r', '')

def function(signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, at = 1, opening + 1
    while depth:
        depth += (source[at] == '{') - (source[at] == '}')
        at += 1
    return source[start:at]

with tempfile.TemporaryDirectory(prefix='tab5-elf-') as temporary:
    folder = pathlib.Path(temporary)
    if args.loader:
        signatures = ['bool validPath(', 'size_t cacheMissing()', 'bool createCache(', 'void cleanup()',
                      'void task(', 'bool yieldValidation()', 'bool loadApplication(', 'void launchTask(',
                      'bool tab5ExecutableRun(', 'void tab5ExecutableStop()', 'void tab5ExecutableService()',
                      'bool tab5ExecutableBusy()', 'bool tab5ExecutableTakeLaunchError()',
                      'const char* tab5ExecutableConsoleSnapshot()', 'void tab5ExecutableClearOutput()']
        (folder / 'elf-loader-production.inc').write_text('\n'.join(function(s) for s in signatures))
        harness = 'elf_loader.cpp'
    else:
        (folder / 'elf-task-production.inc').write_text(function('void task('))
        (folder / 'elf-service-production.inc').write_text(function('void tab5ExecutableService()'))
        harness = 'elf_lifecycle.cpp'
    binary = folder / 'test'
    subprocess.run(['g++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-I', str(folder), '-I', str(args.sketch),
                    str(pathlib.Path(__file__).parent / harness), '-o', str(binary)], check=True)
    run = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
    print(run.stdout, end='')
    if args.report:
        paths = [args.sketch / name for name in ['Tab5Executable.cpp', 'Tab5Executable.h', 'Tab5AppApi.h', 'Tab5Task.h']]
        paths += [pathlib.Path(__file__), pathlib.Path(__file__).parent / harness]
        args.report.write_text(json.dumps({'passed': True, 'physicalDeviceTested': False,
            'loader': args.loader, 'output': run.stdout.strip(),
            'scope': 'Exact production functions with mocked SDK/FS/relocation/admission/audio boundaries and explicit interleavings. Does not execute ELF binaries or emulate the ESP32 scheduler. ASan/UBSan; tracked allocations/files/storage leases; LSan unavailable under executor ptrace.',
            'sourceHashes': {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}}, indent=2) + '\n')
