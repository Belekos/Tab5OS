"""Compile the changed resource/Pentest units for ESP32-P4, without linking or flashing."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import re

here = Path(__file__).resolve().parent
release = here.parents[1]
sketch = release / 'Tab5OS_Arduino_v0.8.26_Ecriture'
build = Path('C:/workspace/tab5-memory-0826')
build.mkdir(parents=True, exist_ok=True)
database_path = Path(sys.argv[1] if len(sys.argv) > 1 else 'C:/workspace/tab5-build-0825-clang/compile_commands.json')
database = json.loads(database_path.read_text())
template = next(row for row in database if Path(row['file']).name == 'Tab5Tools.cpp')
arguments = []
skip = False
for argument in template['arguments']:
    if skip:
        skip = False
        continue
    if argument == '-o':
        skip = True
        continue
    if argument in ('-c', '-MMD', '-fstack-usage', template['file']):
        continue
    argument=re.sub(r'Tab5OS_Ecriture_v0\.8\.\d+', release.name, argument)
    argument=re.sub(r'Tab5OS_Arduino_v0\.8\.\d+_Ecriture', sketch.name, argument)
    arguments.append(argument)

units = ['Tab5Memory.cpp', 'Tab5ResourceManager.cpp', 'Tab5Diagnostics.cpp',
         'Tab5AppPanel.cpp', 'Tab5Tools.cpp', 'Tab5FileShare.cpp',
         'Tab5NetworkTools.cpp', 'Tab5NetworkHttp.cpp', 'Tab5PrinterDiscovery.cpp',
         'Tab5Sdr.cpp', 'Tab5SdrUi.cpp', 'src/rtl_sdr/usb_adapter.cpp']
results = []
for unit in units:
    output = build / (Path(unit).name + '.o')
    command = arguments + ['-c', '-fstack-usage', '-o', str(output), '-x', 'c++', str(sketch / unit)]
    result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', errors='replace')
    row = dict(source=unit, exitCode=result.returncode, command=command, output=result.stdout + result.stderr, frames=[])
    if result.returncode == 0:
        for line in output.with_suffix('.su').read_text().splitlines():
            fields = line.split('\t')
            if len(fields) == 3:
                row['frames'].append(dict(function=fields[0], bytes=int(fields[1]), kind=fields[2]))
        row['frames'].sort(key=lambda frame: frame['bytes'], reverse=True)
    results.append(row)
    print(unit, result.returncode, flush=True)

source_names = units + ['Tab5Memory.h', 'Tab5ResourceManager.h', 'Tab5Task.h',
                        'Tab5Diagnostics.h', 'Tab5AppPanel.h', 'Tab5ConsoleBuffer.h',
                        'Tab5CredentialUi.inc','Tab5Sdr.h','Tab5SdrDsp.h','Tab5SdrModel.h','Tab5SdrUi.h','src/rtl_sdr/libusb.h']
report = dict(version='0.8.26', passed=all(row['exitCode'] == 0 for row in results),
              method='Compile current .26 translation units using the installed ESP32-P4 Clang toolchain and SDK flags from the .25 compile database; -fstack-usage per-function frames. No linking or flashing; a largest single frame is NOT a full call-chain bound.',
              physicalDeviceTested=False, compileDatabase=str(database_path),
              compileDatabaseSha256=hashlib.sha256(database_path.read_bytes()).hexdigest(),
              results=results,
              sources={name: hashlib.sha256((sketch / name).read_bytes()).hexdigest() for name in source_names},
              tests={Path(__file__).name: hashlib.sha256(Path(__file__).read_bytes()).hexdigest()})
(release / 'tools/verification/memory-target-stack-0826.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
