"""Exercise the real gate with deterministic scheduler/semaphore transitions."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--cxx', default='cl')
parser.add_argument('--report', type=Path)
args = parser.parse_args()
here = Path(__file__).resolve().parent
release = here.parent.parent
sketch = release / 'Tab5OS_Arduino_v0.8.22_Ecriture'
source = sketch / 'Tab5NetworkGate.cpp'
with tempfile.TemporaryDirectory(prefix='tab5-gate-') as directory:
    executable = Path(directory) / 'gate.exe'
    if Path(args.cxx).stem.lower() == 'cl':
        command = [args.cxx, '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX', '/utf-8',
                   '/I' + str(here), '/I' + str(sketch),
                   str(here / 'gate.cpp'), str(source), '/Fe:' + str(executable)]
    else:
        command = [args.cxx, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                   '-I' + str(here), '-I' + str(sketch),
                   str(here / 'gate.cpp'), str(source), '-o', str(executable)]
    subprocess.run(command, cwd=directory, check=True)
    result = subprocess.run([str(executable)], check=True, capture_output=True, text=True)
    print(result.stdout, end='')
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps({
            'release': '0.8.22', 'status': 'PASS', 'passed': True,
            'scope': 'host simulation of admission races; no packet-routing or hardware test',
            'sources': {name: hashlib.sha256((sketch / name).read_bytes()).hexdigest()
                        for name in ['Tab5NetworkGate.cpp', 'Tab5NetworkGate.h']},
            'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
            'result': result.stdout.strip()
        }, indent=2) + '\n', encoding='utf-8')
