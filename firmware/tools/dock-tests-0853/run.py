#!/usr/bin/env python3
"""Actual sidebar widgets and launch dispatcher, real LVGL/Focus/USB-mouse boundary."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/settings-ui-tests-0841'))
from release_config import SKETCH, VERSION
from extract import extract

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment', type=Path, required=True)
    args = parser.parse_args()
    env = args.environment.resolve()
    sketch = ROOT / SKETCH
    here = Path(__file__).parent
    build = env / 'dock-tests'
    build.mkdir(parents=True, exist_ok=True)
    evidence = ROOT / 'tools/verification/dock-0853'
    evidence.mkdir(parents=True, exist_ok=True)
    compiled_sources = ['Tab5Focus.cpp', 'Tab5UiStyle.cpp', 'Tab5TextInput.cpp', 'WriterDocument.cpp']
    production = [SKETCH + '/' + name for name in compiled_sources + [
        SKETCH+'.ino', 'Tab5HomeUi.h', 'Tab5ShellDock.h', 'Tab5ShellUi.h', 'Tab5ShellLayout.h',
        'Tab5ShellPolicy.h', 'Tab5AppCataloguePolicy.h', 'Tab5AppCatalogue.h', 'Tab5Icons.h',
        'Tab5Mouse.cpp', 'Tab5Mouse.h', 'Tab5Focus.h', 'Tab5UiStyle.h', 'Tab5TextInput.h',
        'WriterKeys.h', 'WriterMouse.h', 'WriterUsb.h', 'WriterDocument.h', 'Tab5OSTypes.h',
        'Tab5TopBarLayout.h', 'sdcard/System/font.ttf']]
    source_hashes = {path: sha(ROOT / path) for path in production}
    tests = [*here.glob('*.py'), *here.glob('*.cpp'), *here.glob('*.h'), ROOT / 'tools/settings-ui-tests-0841/extract.py']
    test_hashes = {p.relative_to(ROOT).as_posix(): sha(p) for p in tests}
    ino = (sketch / (SKETCH+'.ino')).read_text()
    home = (sketch / 'Tab5HomeUi.h').read_text()
    extracted = {
        'helpers.inc': [extract(ino, f) for f in ['makeButton', 'makeIconButton', 'makeSettingsCard',
                          'makeSettingsText', 'styleSettingsSwitch', 'setText', 'setTopActionVisible']],
        'home.inc': [extract(home, f) for f in ['homeSymbol', 'homePassive', 'homeLabel', 'homeLaunchApp']],
        'main.inc': [extract(ino, f) for f in ['updateNavState', 'dockEvent', 'createDock']],
    }
    for name, parts in extracted.items():
        (build / name).write_text('\n\n'.join(parts)+'\n')
    command = ['g++', '-std=c++17', '-g', '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie',
               '-Wno-deprecated-declarations', '-Wno-cpp', f'-DLV_CONF_PATH="{env / "host-lvgl/lv_conf.h"}"',
               '-I'+str(here), '-I'+str(sketch), '-I'+str(env / 'host-deps/lvgl-9.6.0'), '-I'+str(build),
               str(here / 'ui.cpp'), *[str(sketch / p) for p in compiled_sources],
               str(env / 'host-lvgl/liblvgl.a'), '-o', str(build / 'test')]
    result = subprocess.run(command, capture_output=True, text=True)
    (evidence / 'compile.log').write_text(result.stdout+result.stderr)
    if result.returncode:
        print(result.stderr)
        result.check_returncode()
    result = subprocess.run([str(build / 'test'), str(sketch / 'sdcard/System/font.ttf'), str(evidence)],
                            capture_output=True, text=True, timeout=90,
                            env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',
                                     UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
    (evidence / 'run.log').write_text(result.stdout+result.stderr)
    print(result.stdout+result.stderr)
    result.check_returncode()
    measured=json.loads(result.stdout)
    from PIL import Image
    for path in evidence.glob('*.ppm'):
        Image.open(path).save(path.with_suffix('.png'))
        path.unlink()
    assert all(sha(ROOT/path)==digest for path,digest in {**source_hashes,**test_hashes}.items()), 'Sources changed during test; rerun'
    report={'version':VERSION,'passed':True,'physicalDeviceTested':False,'scope':__doc__,
            'result':measured,'compilerCommand':command,'compileExitCode':0,'runExitCode':0,
            'sanitizers':['address','undefined'],'leakDetection':False,
            'sourceHashes':source_hashes,'testSourceHashes':test_hashes,
            'evidenceHashes':{p.relative_to(ROOT).as_posix():sha(p) for p in evidence.iterdir() if p.is_file()},
            'extractHashes':{name:sha(build/name) for name in extracted},
            'dependencies':{'lvglArchiveSha256':sha(env/'host-lvgl/liblvgl.a'),
                            'lvglConfigSha256':sha(env/'host-lvgl/lv_conf.h')},
            'limitations':['Display driver, USB hardware, application runtimes, filesystem worker and NVS storage are boundary simulations.',
                           'Production shell policies/widgets, stable-key launch dispatcher, actual LVGL rendering and OS pointer/focus code are executed.',
                           'LeakSanitizer is disabled in this environment; repeated object cycles and ASan/UBSan are exercised, not hardware heap measurements.',
                           'No timing, real NVS power-loss, SD removal, native ELF isolation or target audio/display claim is inferred.']}
    (ROOT/'tools/verification/dock-0853.json').write_text(json.dumps(report,indent=2)+'\n')

if __name__=='__main__':
    main()
