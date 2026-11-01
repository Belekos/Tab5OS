"""Linux host checks: production autosave/sleep and battery control, mocked boundaries.
The battery policy/hardware scenarios reuse the unchanged 0.8.28 test fixtures.
"""
from pathlib import Path
import argparse, datetime, hashlib, json, os, re, subprocess, tempfile

here = Path(__file__).resolve().parent
release = here.parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--sketch', type=Path)
args = parser.parse_args()
sketch = args.sketch or next(iter(sorted(release.glob('Tab5OS_Arduino_v0.8.*_Ecriture'), reverse=True)))
legacy = release / 'tools/battery-protection-tests-0828'

def block(text, signature):
    begin = text.index(signature)
    start = text.index('{', begin)
    depth = 0
    quote = None
    escape = line = comment = False
    i = start
    while i < len(text):
        c, pair = text[i], text[i:i+2]
        if line:
            if c == '\n': line = False
        elif comment:
            if pair == '*/': comment = False; i += 1
        elif quote:
            if escape: escape = False
            elif c == '\\': escape = True
            elif c == quote: quote = None
        elif pair == '//': line = True; i += 1
        elif pair == '/*': comment = True; i += 1
        elif c in '\"\'': quote = c
        elif c == '{': depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0: return text[begin:i+1]
        i += 1
    raise ValueError(signature)

source = (sketch / 'Tab5Writing.cpp').read_text('utf8')
header = (sketch / 'Tab5Writing.h').read_text('utf8')
ui = (sketch / 'Tab5BatteryUi.inc').read_text('utf8')
# Retired recovery code must have no owner, caller or file-deletion migration.
for token in ('RecoverySave', 'recoverySave', 'recoveryFs', 'recoveryJob', '/System/writing/recovery'):
    assert token not in source + header + ui, token
assert 'Tab5Writing' not in ui
paths = [sketch/n for n in ('Tab5Writing.cpp','Tab5Writing.h','Tab5BatteryGuard.cpp','Tab5BatteryGuard.h','Tab5BatteryUi.inc')]
paths += [legacy/n for n in ('battery.cpp','M5Unified.h')]
paths += [here/n for n in ('run.py','writing.cpp','battery_ui.cpp')]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before = {str(p.relative_to(release)): sha(p) for p in paths}
runs = []
with tempfile.TemporaryDirectory(prefix='tab5-autosave-0830-') as temp:
    build = Path(temp)
    test = (here / 'writing.cpp').read_text('utf8')
    for name, signature in [('service','void Tab5WritingApp::service('), ('storageIdle','bool Tab5WritingApp::storageIdle()'), ('prepareSleep','bool Tab5WritingApp::prepareSleep()')]:
        test = test.replace('/* production_' + name + ' */', block(source,signature))
    generated = build / 'writing.cpp'
    generated.write_text(test, encoding='utf8')
    compiler = os.environ.get('CXX','g++')
    common = [compiler,'-std=c++17','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(sketch)]
    targets = [('writing',[generated]),('battery_ui',[here/'battery_ui.cpp']),('battery',[legacy/'battery.cpp',sketch/'Tab5BatteryGuard.cpp'])]
    for name, inputs in targets:
        exe = build/name
        command = common + (['-DCONFIG_IDF_TARGET_ESP32P4','-I'+str(legacy)] if name=='battery' else []) + list(map(str,inputs)) + ['-o',str(exe)]
        compiled = subprocess.run(command,capture_output=True,text=True)
        if compiled.returncode: raise RuntimeError(compiled.stdout+compiled.stderr)
        for scenario in ([None]+list(range(18)) if name=='battery' else [None]):
            result = subprocess.run([str(exe)]+([] if scenario is None else [str(scenario)]),capture_output=True,text=True,timeout=30)
            output = result.stdout + result.stderr
            runs.append(dict(test=name, scenario='default' if scenario is None else scenario, exitCode=result.returncode, output=output))
            print(name,scenario,output.strip())
            if result.returncode: raise RuntimeError(output)
    compiler_version = subprocess.check_output([compiler,'--version'],text=True).splitlines()[0]
report = dict(version='0.8.30', passed=before=={str(p.relative_to(release)):sha(p) for p in paths},
              checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              physicalDeviceTested=False, fullFirmwareCompiled=False,
              method='Complete production writing service/storageIdle/prepareSleep methods extracted with storage and UI doubles; exact BatteryUi include with display/sensor doubles, intentionally no writing API. Unchanged production battery guard and prior sensor/voltage/charging/time-wrap fixtures. Linux C++17 with UndefinedBehaviorSanitizer; no LVGL rendering or board test.',
              compiler=compiler_version, checks=sum(int(m) for r in runs for m in re.findall(r'PASS (\d+) checks',r['output'])),
              sourceHashes=before,runs=runs)
(release/'tools/verification/autosave-battery-0830.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('PASS' if report['passed'] else 'FAIL',report['checks'],'checks')
raise SystemExit(0 if report['passed'] else 1)
