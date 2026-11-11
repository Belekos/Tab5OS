#!/usr/bin/env python3
"""Compile the production HTTP audio body reader on fragmented wire fixtures."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
FIRMWARE = HERE.parents[2]
REPOSITORY = FIRMWARE.parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path)
parser.add_argument('--cxx', default=os.environ.get('CXX', 'cl' if os.name == 'nt' else 'c++'))
args = parser.parse_args()
msvc = Path(args.cxx).stem.lower() == 'cl'
with tempfile.TemporaryDirectory(prefix='tab5-radio-body-') as temporary:
    work = Path(temporary)
    executable = work / ('radio_body.exe' if os.name == 'nt' else 'radio_body')
    if msvc:
        command = [args.cxx, '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX', '/I' + str(FIRMWARE),
                   str(HERE / 'test_body.cpp'), '/Fe:' + str(executable)]
    else:
        command = [args.cxx, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g', '-O1',
                   '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined', '-fno-omit-frame-pointer',
                   '-I' + str(FIRMWARE), str(HERE / 'test_body.cpp'), '-o', str(executable)]
    subprocess.run(command, cwd=work, check=True)
    result = subprocess.run([str(executable)], cwd=work, check=True, capture_output=True, text=True)
    print(result.stdout, end='')
    match = re.search(r'RADIO HTTP BODY PASS checks=(\d+)', result.stdout)
    if not match:
        raise SystemExit('HTTP body regression did not produce its success marker')
sources = [FIRMWARE / 'Tab5RadioBodyReader.h', HERE / 'test_body.cpp', Path(__file__)]
report = {'component': 'radio-http-body', 'passed': True, 'checks': int(match[1]),
          'sanitizers': [] if msvc else ['AddressSanitizer', 'UndefinedBehaviorSanitizer'],
          'stdout': result.stdout,
          'sourceHashes': {path.relative_to(REPOSITORY).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                           for path in sources},
          'limitations': ['Wire framing fixtures use a nonblocking host socket double; no physical Wi-Fi or radio decoder timing measurement.']}
if args.output:
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
