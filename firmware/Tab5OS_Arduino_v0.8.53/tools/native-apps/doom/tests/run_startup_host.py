#!/usr/bin/env python3
"""Build the complete current Doom engine on Linux and test Tab5 startup services."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
DOOM = HERE.parent
SKETCH = DOOM.parents[2]
REPO = SKETCH.parent.parent
MATH_NAMES = ('sin cos tan atan atan2 acos asin sqrt floor ceil fmod pow exp log ldexp '
              'sinf cosf tanf atanf atan2f acosf asinf sqrtf floorf ceilf fmodf powf expf logf ldexpf fabsf').split()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wad', type=Path, required=True, help='Test-only Doom shareware IWAD')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if sys.platform != 'linux':
        parser.error('This integration runner requires Linux GCC and objcopy')
    captured = []
    with patch.object(sys, 'argv', [str(DOOM / 'build.py')]), \
         patch.object(subprocess, 'run', lambda command, **kwargs: captured.append(command)):
        runpy.run_path(str(DOOM / 'build.py'), run_name='__main__')
    command = captured[0]
    sources = [Path(command[2])]
    definitions, includes = [], []
    for index, item in enumerate(command):
        if item == '--extra-source': sources.append(Path(command[index + 1]))
        elif item == '--define': definitions.append('-D' + command[index + 1])
        elif item == '--include': includes.append('-I' + command[index + 1])
    compiler = os.environ.get('CC', 'gcc')
    flags = ['-std=gnu11', '-g', '-O1', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0',
             '-fno-builtin', '-fno-omit-frame-pointer', '-fsanitize=address',
             '-DTAB5_HOST_TEST', '-I' + str(SKETCH), *includes, *definitions]
    results = []
    with tempfile.TemporaryDirectory(prefix='tab5-doom-startup-') as temp:
        work = Path(temp)
        objects = []
        for index, source in enumerate(sources):
            obj = work / ('%03d.o' % index)
            built = subprocess.run([compiler, *flags, '-c', str(source), '-o', str(obj)],
                                   text=True, capture_output=True)
            if built.returncode:
                print(built.stdout + built.stderr)
                built.check_returncode()
            subprocess.run(['objcopy', *['--redefine-sym=' + name + '=tab5_rt_' + name
                                        for name in MATH_NAMES], str(obj)], check=True)
            objects.append(obj)
        executable = work / 'doom-startup'
        subprocess.run([compiler, '-g', '-fsanitize=address', '-fno-omit-frame-pointer',
                        '-I' + str(SKETCH), str(HERE / 'startup_host.c'),
                        *map(str, objects), '-lm', '-o', str(executable)], check=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:detect_stack_use_after_return=0')
        for scenario in ('title', 'play', 'missing', 'no-argument', 'oom', 'video-failure'):
            saves = work / scenario
            saves.mkdir()
            result = subprocess.run([str(executable), str(args.wad.resolve()), str(saves), scenario],
                                    env=env, text=True, capture_output=True, timeout=180)
            log = args.output.with_name(args.output.stem + '-' + scenario + '.log')
            log.write_text(result.stdout + result.stderr, encoding='utf-8')
            print(scenario + ': ' + (result.stdout.strip().splitlines()[-1] if result.stdout else result.stderr[-500:]))
            result.check_returncode()
            results.append(json.loads(result.stdout.strip().splitlines()[-1]))
    report = {'passed': True, 'component': 'Complete Doom host startup', 'scenarios': results,
              'wadSha256': hashlib.sha256(args.wad.read_bytes()).hexdigest(),
              'sanitizers': ['AddressSanitizer'],
              'sourceHashes': {p.relative_to(REPO).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in [*sources, HERE / 'startup_host.c', Path(__file__)]},
              'limitations': ['Runs the production engine/runtime sources on host pointers with mocked OS services; actual ELF instruction/loading behavior is covered separately.']}
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(args.output)


if __name__ == '__main__':
    main()
