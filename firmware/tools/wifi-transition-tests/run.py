"""Execute production Wi-Fi methods, popup rendering and gate with driver stubs.

The radio/FreeRTOS/LVGL boundary is simulated. Function bodies are extracted
from the actual release files and are not reimplemented in this test.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--baseline', type=Path)
parser.add_argument('--report', type=Path)
args = parser.parse_args()
here = Path(__file__).resolve().parent
release = here.parent.parent
sketch = release / 'Tab5OS_Arduino_v0.8.25_Ecriture'
gate_stubs = release / 'tools/network-gate-tests'


def block(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    position = opening + 1
    while depth:
        if text[position] == '{':
            depth += 1
        elif text[position] == '}':
            depth -= 1
        position += 1
    return text[start:position]


def build_and_run(folder, baseline=False):
    audio = (folder / 'Tab5Audio.cpp').read_text(encoding='utf-8')
    ino = next(folder.glob('*.ino'))
    ui = ino.read_text(encoding='utf-8')
    sharing = (folder / 'Tab5FileShare.cpp').read_text(encoding='utf-8')
    signatures = [
        'size_t Tab5AudioEngine::wifiNetworkCount()',
        'bool Tab5AudioEngine::wifiNetwork(',
        'bool Tab5AudioEngine::wifiScanRunning()',
        'uint32_t Tab5AudioEngine::wifiScanRevision()',
        'bool Tab5AudioEngine::execStartWifiScan()',
        'void Tab5AudioEngine::serviceWifiScan()',
        'bool Tab5AudioEngine::execConnectWifi(',
        'bool Tab5AudioEngine::execSetWifiEnabled(',
        'bool Tab5AudioEngine::startWifiScan()',
    ]
    if not baseline:
        signatures += ['String Tab5AudioEngine::wifiScanStatus()',
                       'void Tab5AudioEngine::setWifiScanStatus(',
                       'void Tab5AudioEngine::serviceWifiConnection()']
    pieces = [block(audio, signature) for signature in signatures]
    pieces += [block(ui, 'static void refreshWifiPopupList(bool force) {')]
    if baseline:
        # Compile the original AP admission and driver calls unchanged. The
        # enclosing server's cleanup label returns here instead of freeing a
        # socket/context, because a failed admission has neither resource.
        start = sharing.index('    Tab5NetworkLease transition("partage Wi-Fi"')
        end = sharing.index('    c.address = ', start)
        pieces += ['static bool baselineAccessPointStart() {\n'
                   'bool apAttempted = false; const char* lastError = "";\n'
                   'const char* shareSsid = "Tab5-test"; const char* sharePassword = "test-password";\n'
                   + sharing[start:end] + '\nreturn apAttempted;\n'
                   'cleanup: fprintf(stderr, "AP admission: %s\\n", lastError); return false;\n}']
    else:
        pieces += [block(sharing, 'class Tab5FileShareNetworkTransition {') + ';']
        pieces += [block(sharing, 'void finishSession('), block(sharing, 'void tab5FileShareStop()')]
        assert 'finishSession(c, server, apAttempted);' in block(sharing, 'void worker(')
    with tempfile.TemporaryDirectory(prefix='tab5-wifi-flow-') as directory:
        temp = Path(directory)
        (temp / 'production-flow.inc').write_text('\n\n'.join(pieces), encoding='utf-8')
        executable = temp / 'flow.exe'
        # Original translation units are combined here, so their local names
        # can shadow another unit's otherwise-invisible namespace state.
        command = ['cl', '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX', '/wd4505', '/wd4459', '/utf-8',
                   '/I' + str(temp), '/I' + str(here), '/I' + str(gate_stubs), '/I' + str(folder)]
        if baseline:
            command += ['/DTAB5_BASELINE']
        command += [str(here / 'flow.cpp'), str(folder / 'Tab5NetworkGate.cpp'), '/Fe:' + str(executable)]
        subprocess.run(command, cwd=temp, check=True)
        result = subprocess.run([str(executable)], cwd=temp, capture_output=True, text=True, encoding='utf-8')
        print(result.stdout, end='')
        print(result.stderr, end='')
        if baseline:
            assert result.returncode == 1 and result.stdout.count('EXPECTED-FAIL:') == 3, result
        else:
            assert result.returncode == 0, result
        files = ['Tab5Audio.cpp', 'Tab5Audio.h', 'Tab5NetworkGate.cpp', 'Tab5NetworkGate.h', 'Tab5FileShare.cpp', ino.name]
        return {'passed': not baseline, 'expected_failure_observed': baseline,
                'exit_code': result.returncode, 'output': result.stdout + result.stderr,
                'sources': {name: hashlib.sha256((folder / name).read_bytes()).hexdigest() for name in files}}


report = {'version': '0.8.25', 'scope': 'production functions with simulated driver/scheduler/UI boundaries; no hardware test'}
if args.baseline:
    report['baseline_0_8_19'] = build_and_run(args.baseline.resolve(), True)
report['current'] = build_and_run(sketch)
report['passed'] = True
report['sources'] = report['current']['sources']
test_sources = [here / name for name in ('run.py', 'run_msvc.cmd', 'flow.cpp', 'Platform.h')]
test_sources += sorted(gate_stubs.rglob('*.h'))
report['testSources'] = {path.relative_to(release).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in test_sources}
if args.report:
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
