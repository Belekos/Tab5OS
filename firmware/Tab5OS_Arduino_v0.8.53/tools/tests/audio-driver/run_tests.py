#!/usr/bin/env python3
"""Compile the actual common PCM driver against a two-pointer Speaker model."""
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
with tempfile.TemporaryDirectory(prefix='tab5-audio-driver-') as temporary:
    work = Path(temporary)
    executable = work / ('audio_driver.exe' if os.name == 'nt' else 'audio_driver')
    if msvc:
        command = [args.cxx, '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX',
                   '/I' + str(HERE / 'stubs'), '/I' + str(FIRMWARE),
                   str(HERE / 'test_audio_driver.cpp'), str(FIRMWARE / 'Tab5AudioDriver.cpp'),
                   '/Fe:' + str(executable)]
    else:
        command = [args.cxx, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g', '-O1',
                   '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                   '-I' + str(HERE / 'stubs'), '-I' + str(FIRMWARE),
                   str(HERE / 'test_audio_driver.cpp'), str(FIRMWARE / 'Tab5AudioDriver.cpp'),
                   '-o', str(executable)]
    subprocess.run(command, cwd=work, check=True)
    result = subprocess.run([str(executable)], cwd=work, check=True, capture_output=True, text=True)
    print(result.stdout, end='')
    match = re.search(r'AUDIO DRIVER PASS checks=(\d+):', result.stdout)
    if not match:
        raise SystemExit('Common PCM driver test did not produce its success marker')
sources = [FIRMWARE / 'Tab5AudioDriver.cpp', FIRMWARE / 'Tab5AudioDriver.h',
           FIRMWARE / 'Tab5Serial.h', FIRMWARE / 'Tab5SerialCore.h',
           HERE / 'test_audio_driver.cpp', Path(__file__),
           *sorted(path for path in (HERE / 'stubs').rglob('*') if path.is_file())]
report = {'passed': True, 'component': 'Common PCM driver buffering and lifetime',
          'checks': int(match[1]), 'sanitizers': [] if msvc else ['AddressSanitizer', 'UndefinedBehaviorSanitizer'],
          'stdout': result.stdout,
          'sourceHashes': {path.relative_to(REPOSITORY).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                           for path in sources},
          'limitations': ['Host Speaker queue model; no physical I2S timing measurement.']}
if args.output:
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
