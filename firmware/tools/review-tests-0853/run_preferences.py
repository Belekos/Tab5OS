#!/usr/bin/env python3
"""Execute production sidebar persistence with an in-memory Preferences boundary."""
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
    sources = [ROOT / SKETCH / name for name in ('Tab5ShellLayout.h', 'Tab5ShellPolicy.h', 'Tab5AppCataloguePolicy.h')]
    tests = [Path(__file__), Path(__file__).with_name('preferences.cpp'), Path(__file__).with_name('Preferences.h')]
    source_hashes = {p.relative_to(ROOT).as_posix(): sha(p) for p in sources}
    test_hashes = {p.relative_to(ROOT).as_posix(): sha(p) for p in tests}
    output = env / 'review-preferences'
    command = ['g++', '-std=c++17', '-g', '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie',
               '-Wno-deprecated-declarations', '-Wno-cpp', f'-DLV_CONF_PATH="{env / "host-lvgl/lv_conf.h"}"',
               '-I' + str(Path(__file__).parent), '-I' + str(ROOT / SKETCH), '-I' + str(env / 'host-deps/lvgl-9.6.0'),
               str(Path(__file__).with_name('preferences.cpp')), '-o', str(output)]
    compile_result = subprocess.run(command, capture_output=True, text=True)
    (directory / 'preferences-compile.log').write_text(compile_result.stdout + compile_result.stderr)
    assert compile_result.returncode == 0, 'Compile failed; inspect preferences-compile.log'
    runtime_env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
    result = subprocess.run([str(output)], capture_output=True, text=True, env=runtime_env)
    (directory / 'preferences-run.log').write_text(result.stdout + result.stderr)
    assert result.returncode == 0, 'Execution failed; inspect preferences-run.log'
    measured = json.loads(result.stdout)
    assert measured['passed']
    assert all(sha(ROOT / p) == digest for p, digest in {**source_hashes, **test_hashes}.items())
    report = {'version': VERSION, 'passed': True, 'physicalDeviceTested': False,
              'scope': __doc__, 'result': measured,
              'compilerCommand': command, 'compileExitCode': compile_result.returncode, 'runExitCode': result.returncode,
              'sanitizers': ['address', 'undefined'], 'leakDetection': False,
              'sourceHashes': source_hashes, 'testSourceHashes': test_hashes,
              'evidenceHashes': {p.relative_to(ROOT).as_posix(): sha(p) for p in directory.glob('preferences-*.log')},
              'limitations': ['NVS/Flash hardware is simulated. No power-loss guarantee is inferred.',
                              'LeakSanitizer is disabled because this execution environment cannot inspect its process tasks.',
                              'This test executes production load/save and Choices validation; UI/controller lifecycle is reviewed separately.']}
    (ROOT / 'tools/verification/sidebar-persistence-review-0853.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'passed': True, 'checks': measured['checks'], 'sourcesStable': True}))

if __name__ == '__main__':
    main()
