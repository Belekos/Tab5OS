"""Run byte-for-byte production firmware logic against simulated file/OTA APIs."""
from pathlib import Path
import argparse, hashlib, json, shutil, subprocess, tempfile

p = argparse.ArgumentParser()
p.add_argument('sketch', type=Path)
p.add_argument('--cxx', default='cl')
p.add_argument('--report', type=Path)
a = p.parse_args()
here = Path(__file__).resolve().parent
sketch = a.sketch.resolve()
source = sketch / 'Tab5Firmware.cpp'
scenarios = ['direct', 'reviewed', 'changed-size', 'changed-hash', 'same-path-case',
             'different-file', 'clear-review', 'invalid-path', 'missing-file',
             'wrong-chip', 'wrong-revision', 'truncated', 'oversize', 'begin-failure',
             'write-failure', 'integrity-failure', 'cancel', 'busy-storage',
             'task-failure', 'usb-file']
results = []
with tempfile.TemporaryDirectory(prefix='tab5-firmware-') as temporary:
    work = Path(temporary)
    for original in [source, here/'Platform.h', here/'session.cpp']:
        shutil.copy2(original, work/original.name)
    for header in ['Tab5VolumeFS.h', 'Tab5Storage.h', 'Tab5Task.h', 'Arduino.h',
                   'SD.h', 'esp_ota_ops.h', 'esp_app_format.h', 'esp_app_desc.h',
                   'esp_chip_info.h', 'esp_system.h', 'mbedtls/sha256.h']:
        target = work/header
        target.parent.mkdir(exist_ok=True)
        target.write_text('#include "Platform.h"\n', encoding='utf-8')
    subprocess.run([a.cxx, '/nologo', '/std:c++17', '/EHsc', '/W4', '/utf-8',
                    '/I'+str(work), '/I'+str(sketch), 'session.cpp', '/Fe:session.exe'],
                   cwd=work, check=True)
    for scenario in scenarios:
        result = subprocess.run([str(work/'session.exe'), scenario], cwd=work,
                                capture_output=True, text=True)
        print(result.stdout, end='')
        results.append({'scenario': scenario, 'exitCode': result.returncode,
                        'stdout': result.stdout, 'stderr': result.stderr})
report = {'source': 'Tab5Firmware.cpp', 'sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
          'scope': 'Production OTA state flow with mocked filesystem, digest and ESP OTA APIs; no hardware flashing.',
          'cases': results, 'passed': all(r['exitCode'] == 0 for r in results)}
if a.report:
    a.report.parent.mkdir(parents=True, exist_ok=True)
    a.report.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print('Production source SHA256:', report['sha256'])
raise SystemExit(0 if report['passed'] else 1)
