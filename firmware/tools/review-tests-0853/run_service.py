#!/usr/bin/env python3
"""Run the extracted production catalogue scheduler against observable scanner boundaries."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from release_config import SKETCH, VERSION

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment', type=Path, required=True)
    args = parser.parse_args()
    env = args.environment.resolve()
    directory = ROOT / 'tools/verification/review-0853'
    directory.mkdir(parents=True, exist_ok=True)
    sources = [ROOT / SKETCH / name for name in ('Tab5HomeUi.h', 'Tab5AppRefresh.h')]
    tests = [Path(__file__), Path(__file__).with_name('service.cpp')]
    source_hashes = {p.relative_to(ROOT).as_posix(): sha(p) for p in sources}
    test_hashes = {p.relative_to(ROOT).as_posix(): sha(p) for p in tests}
    text = sources[0].read_text()
    start = text.index('static void serviceHomeApps(){')
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        if text[end] == '{': depth += 1
        elif text[end] == '}': depth -= 1
        end += 1
    (directory / 'service.inc').write_text(text[start:end] + '\n')
    output = env / 'review-service'
    command = ['g++', '-std=c++17', '-g', '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie',
               '-I' + str(directory), '-I' + str(ROOT / SKETCH), str(Path(__file__).with_name('service.cpp')), '-o', str(output)]
    compiled = subprocess.run(command, capture_output=True, text=True)
    (directory / 'service-compile.log').write_text(compiled.stdout + compiled.stderr)
    assert compiled.returncode == 0, 'Compile failed; inspect service-compile.log'
    runs = []
    runtime_env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
    for scenario in ('native-idle', 'external-idle', 'support', 'catalogue', 'cancel', 'settings'):
        result = subprocess.run([str(output), scenario], capture_output=True, text=True, env=runtime_env)
        (directory / f'service-{scenario}-run.log').write_text(result.stdout + result.stderr)
        assert result.returncode == 0, f'{scenario} failed; inspect log'
        runs.append(json.loads(result.stdout))
        assert runs[-1]['passed']
    assert all(sha(ROOT / p) == digest for p, digest in {**source_hashes, **test_hashes}.items())
    report = {'version': VERSION, 'passed': True, 'physicalDeviceTested': False,
              'scope': __doc__, 'checks': sum(r['checks'] for r in runs), 'scenarios': runs,
              'compilerCommand': command, 'sanitizers': ['address', 'undefined'], 'leakDetection': False,
              'sourceHashes': source_hashes, 'testSourceHashes': test_hashes,
              'evidenceHashes': {p.relative_to(ROOT).as_posix(): sha(p) for p in directory.glob('service*')},
              'limitations': ['Production serviceHomeApps and Schedule run unchanged; catalogue worker, storage, LVGL rendering and task boundaries are simulated.',
                              'Time is a deterministic counter; no physical I/O latency, task scheduling or flash timing is measured.',
                              'LeakSanitizer is disabled because this execution environment cannot inspect its process tasks.']}
    (ROOT / 'tools/verification/catalogue-service-review-0853.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'passed': True, 'checks': report['checks'], 'sourcesStable': True}))

if __name__ == '__main__':
    main()
