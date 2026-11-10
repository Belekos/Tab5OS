#!/usr/bin/env python3
"""Exercise production mouse capture inside actual LVGL 9.6 input callbacks."""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
FIRMWARE = HERE.parents[2]
REPOSITORY = FIRMWARE.parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, work):
    result = subprocess.run(command, cwd=work, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError('Command failed: ' + str(command[0]) + '\n' + result.stdout + result.stderr)
    return result


def validate(args, work):
    lvgl = args.lvgl.resolve()
    properties = lvgl / 'library.properties'
    if 'version=9.6.0' not in properties.read_text(encoding='utf8').splitlines():
        raise RuntimeError('Expected pinned LVGL 9.6.0')
    cc = os.environ.get('CC', 'cl' if os.name == 'nt' else 'cc')
    cxx = os.environ.get('CXX', 'cl' if os.name == 'nt' else 'c++')
    msvc = Path(cc).stem.lower() == 'cl'
    if msvc != (Path(cxx).stem.lower() == 'cl'):
        raise RuntimeError('C and C++ compilers must use the same ABI')
    definitions = ['LV_CONF_SKIP', 'LV_USE_STDLIB_MALLOC=1',
                   'LV_LABEL_TEXT_SELECTION=1', 'LV_USE_OS=0']
    includes = [lvgl, lvgl / 'include', FIRMWARE, work]
    if msvc:
        flags = ['/nologo', '/utf-8', '/O1', '/D_CRT_SECURE_NO_WARNINGS']
        flags += ['/D' + value for value in definitions] + ['/I' + str(p) for p in includes]
    else:
        flags = ['-g', '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                 '-Wno-cpp', '-Wno-deprecated-declarations']
        flags += ['-D' + value for value in definitions] + ['-I' + str(p) for p in includes]
    production = FIRMWARE / 'Tab5Mouse.cpp'
    mouse = '\n'.join(line for line in production.read_text(encoding='utf8').splitlines()
                      if not line.lstrip().startswith('#include'))
    (work / 'mouse_service.inc').write_text(mouse, encoding='utf8')
    # Reintroduce only the offending synchronous read in capture(). This must
    # fail with real LVGL; all other production code and test inputs stay equal.
    anchor = ' captureRefreshPending=true;\n}'
    if mouse.count(anchor) != 1:
        raise RuntimeError('Capture regression mutation anchor changed')
    unsafe = mouse.replace(anchor, ' if(input)lv_indev_read(input);\n' + anchor, 1)
    (work / 'unsafe_mouse_service.inc').write_text(unsafe, encoding='utf8')
    library_sources = [p for p in sorted((lvgl / 'src').rglob('*'))
                       if p.suffix in ('.c', '.cpp')
                       and 'thorvg' not in p.parts and 'vg_lite_tvg' not in p.parts]
    library_headers = sorted(p for p in lvgl.rglob('*.h') if p.is_file())
    dependency_hashes = {p.relative_to(lvgl).as_posix(): sha(p)
                         for p in [properties, *library_sources, *library_headers]}
    dependency_digest = hashlib.sha256(json.dumps(dependency_hashes, sort_keys=True).encode()).hexdigest()
    objects = work / 'objects'
    objects.mkdir(exist_ok=True)

    def compile_library(source):
        compiler = cxx if source.suffix == '.cpp' else cc
        obj = objects / ('_'.join(source.relative_to(lvgl).parts) + ('.obj' if msvc else '.o'))
        stamp = obj.with_suffix('.sha256')
        signature = hashlib.sha256((dependency_digest + repr(flags) + compiler).encode()).hexdigest()
        if not obj.exists() or not stamp.exists() or stamp.read_text() != signature:
            if msvc:
                command = [compiler, *flags, '/c', str(source), '/Fo:' + str(obj)]
            else:
                command = [compiler, *flags, '-c', str(source), '-o', str(obj)]
            run(command, work)
            stamp.write_text(signature)
        return obj

    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        compiled = list(pool.map(compile_library, library_sources))
    archive = work / ('lvgl.lib' if msvc else 'liblvgl.a')
    if msvc:
        response = work / 'archive.rsp'
        response.write_text('\n'.join('"' + str(p) + '"' for p in compiled), encoding='utf8')
        run(['lib', '/nologo', '/OUT:' + str(archive), '@' + str(response)], work)
    else:
        run(['ar', 'rcs', str(archive), *map(str, compiled)], work)
    print('Built actual LVGL 9.6.0: %d translation units' % len(compiled), flush=True)
    environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',
                       UBSAN_OPTIONS='halt_on_error=1')
    results, guards = [], []
    for mutation in (False, True):
        name = 'mouse_capture_unsafe' if mutation else 'mouse_capture'
        executable = work / (name + ('.exe' if os.name == 'nt' else ''))
        if msvc:
            command = [cxx, *flags, '/EHsc', '/std:c++17',
                       *(['/DTEST_SYNCHRONOUS_CAPTURE_READ'] if mutation else []),
                       str(HERE / 'test_mouse_capture_lvgl.cpp'), str(archive),
                       '/Fo:' + str(work / (name + '.obj')), '/Fe:' + str(executable)]
        else:
            command = [cxx, *flags, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                       '-Wno-misleading-indentation',
                       *(['-DTEST_SYNCHRONOUS_CAPTURE_READ'] if mutation else []),
                       str(HERE / 'test_mouse_capture_lvgl.cpp'), str(archive), '-lm', '-o', str(executable)]
        run(command, work)
        for scenario in (['touch', 'mouse'] if mutation else ['touch', 'mouse', 'held-release', 'disconnect']):
            result = subprocess.run([str(executable), scenario], cwd=work, env=environment,
                                    text=True, capture_output=True, timeout=15)
            output = (result.stdout + result.stderr).strip()
            if mutation:
                expected = ('capture cleared or replaced active input' if scenario == 'touch'
                            else 'recursive LV_EVENT_CLICKED')
                if result.returncode != 2 or expected not in output:
                    raise RuntimeError('Synchronous capture regression was not detected: ' + output)
                guards.append({'test': scenario, 'result': 'detected', 'exitCode': result.returncode,
                               'output': output})
            else:
                if result.returncode:
                    raise RuntimeError(scenario + ': ' + output)
                results.append({'test': scenario, 'result': 'passed', 'output': output})
            print(('PASS regression guard ' + scenario + ': ' + output) if mutation else output,
                  flush=True)
    sources = [production, FIRMWARE / 'Tab5Mouse.h', FIRMWARE / 'WriterMouse.h',
               HERE / 'test_mouse_capture_lvgl.cpp', HERE / 'run_mouse_capture_lvgl.py']
    return {'component': 'native-mouse-lvgl', 'passed': True, 'lvglVersion': '9.6.0',
            'compiler': cxx, 'tests': results, 'regressionGuards': guards,
            'sourceHashes': {p.relative_to(REPOSITORY).as_posix(): sha(p) for p in sources},
            'lvglSourceHashes': dependency_hashes, 'lvglTranslationUnits': len(compiled),
            'hostDefinitions': definitions,
            'sanitizers': [] if msvc else ['AddressSanitizer', 'UndefinedBehaviorSanitizer'],
            'limitations': ['Actual LVGL input dispatch and production mouse service run on the host.',
                           'USB reports and touchscreen coordinates are injected; physical peripherals, ESP32 reset behaviour and scheduling need device validation.',
                           'Host LVGL uses its default widget configuration, a libc allocator and no OS backend.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lvgl', required=True, type=Path)
    parser.add_argument('--build', type=Path, help='Optional reusable host build directory')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.build:
        work = args.build.resolve()
        work.mkdir(parents=True, exist_ok=True)
        report = validate(args, work)
    else:
        with tempfile.TemporaryDirectory(prefix='tab5-mouse-lvgl-') as temp:
            report = validate(args, Path(temp))
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')


if __name__ == '__main__':
    main()
