#!/usr/bin/env python3
"""Check touchpad/USB routing and boot real CuteMouse through the serial UART."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
DOS = HERE.parent
SKETCH = DOS.parents[2]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--image', type=Path, default=SKETCH / 'sdcard/DOS/FREEDOS.IMG')
    p.add_argument('--drivers', type=Path, default=SKETCH / 'sdcard/System/DOS/DRIVERS.IMG')
    args = p.parse_args()
    args.output = args.output.resolve()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if os.name == 'nt' and not os.environ.get('VSCMD_VER'):
        batch = args.output.with_suffix('.cmd')
        batch.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul\n' +
                         subprocess.list2cmdline([sys.executable, '-B', str(Path(__file__).resolve()),
                         '--output', str(args.output), '--image', str(args.image), '--drivers', str(args.drivers)]) + '\n')
        raise SystemExit(subprocess.call(['cmd', '/d', '/c', str(batch)]))
    reports = []
    with tempfile.TemporaryDirectory(prefix='tab5-dos-mouse-') as folder:
        work = Path(folder)
        frontend = work / ('frontend.exe' if os.name == 'nt' else 'frontend')
        host = work / ('mouse-host.exe' if os.name == 'nt' else 'mouse-host')
        command = (['cl', '/nologo', '/std:c++17', '/EHsc', '/O2', str(HERE / 'mouse_frontend.cpp'), '/Fe:' + str(frontend)]
                   if os.name == 'nt' else ['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', str(HERE / 'mouse_frontend.cpp'), '-o', str(frontend)])
        commands = [command, [sys.executable, '-B', str(HERE / 'build_host.py'),
                    *(['--msvc'] if os.name == 'nt' else []), '--main', str(HERE / 'mouse_host.c'), '-o', str(host)]]
        for index, command in enumerate(commands):
            result = subprocess.run(command, cwd=work, capture_output=True, text=True, encoding='utf8', errors='replace')
            args.output.with_suffix('.build' + str(index) + '.log').write_text(result.stdout + result.stderr, encoding='utf8')
            if result.returncode:
                print(result.stdout + result.stderr)
                result.check_returncode()
        for index, command in enumerate(([str(frontend)], [str(host), str(args.image), str(args.drivers)])):
            result = subprocess.run(command, cwd=work, capture_output=True, text=True, encoding='utf8', errors='replace', timeout=120)
            args.output.with_suffix('.run' + str(index) + '.log').write_text(result.stdout + result.stderr, encoding='utf8')
            print(result.stdout + result.stderr)
            result.check_returncode()
            reports.append(json.loads(result.stdout.strip().splitlines()[-1]))
    result = {'passed': all(r['passed'] for r in reports), 'tests': reports,
              'imageSha256': hashlib.sha256(args.image.read_bytes()).hexdigest(),
              'driversSha256': hashlib.sha256(args.drivers.read_bytes()).hexdigest(),
              'limitations': ['Host adapters exercise the real serial UART and guest driver; no physical USB mouse or touch hardware timing test.']}
    sources = [DOS / 'app_mouse.h', HERE / 'mouse_frontend.cpp', HERE / 'mouse_host.c',
               HERE / 'run_mouse_tests.py', HERE / 'build_host.py', DOS / 'x87_disabled.c',
               SKETCH / 'Tab5AppInputQueue.h', SKETCH / 'Tab5AppApi.h']
    sources += sorted((DOS / 'core').rglob('*.c')) + sorted((DOS / 'core').rglob('*.h'))
    sources += sorted((HERE / 'host-compat').glob('*.h'))
    result['sourceHashes'] = {p.relative_to(SKETCH.parents[1]).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in sources}
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
    print(args.output)


if __name__ == '__main__':
    main()
