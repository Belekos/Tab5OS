#!/usr/bin/env python3
"""Run the actual allocation-free radio codec selector on the host."""
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
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--cxx', default=os.environ.get('CXX', 'cl' if os.name == 'nt' else 'c++'))
args = parser.parse_args()
msvc = Path(args.cxx).stem.lower() == 'cl'
with tempfile.TemporaryDirectory(prefix='tab5-radio-codec-') as temporary:
    work = Path(temporary)
    target = work / ('radio_codec.exe' if os.name == 'nt' else 'radio_codec')
    source = HERE / 'test_codec.cpp'
    command = ([args.cxx, '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX',
                '/I' + str(FIRMWARE), str(source), '/Fe:' + str(target)] if msvc else
               [args.cxx, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g', '-O1',
                '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                '-fno-omit-frame-pointer', '-I' + str(FIRMWARE), str(source), '-o', str(target)])
    subprocess.run(command, cwd=work, check=True)
    result = subprocess.run([str(target)], cwd=work, text=True, capture_output=True, check=True)
    match = re.fullmatch(r'RADIO CODEC PASS checks=(\d+)\s*', result.stdout)
    if not match:
        raise SystemExit('Missing codec regression success marker')
    sources = [FIRMWARE / 'Tab5RadioCodec.h', source, Path(__file__)]
    report = {'component': 'radio-codec-selection', 'passed': True, 'checks': int(match[1]),
              'sanitizers': [] if msvc else ['AddressSanitizer', 'UndefinedBehaviorSanitizer'],
              'sourceHashes': {path.relative_to(REPOSITORY).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                               for path in sources},
              'limitations': ['Codec routing test only; no live server or physical decoder timing measurement.']}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(result.stdout, end='')
