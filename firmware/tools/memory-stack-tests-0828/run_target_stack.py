"""Compile current resource/Pentest units for ESP32-P4 without linking or flashing."""
from pathlib import Path
import datetime
import hashlib
import json
import subprocess
import sys
import re

here = Path(__file__).resolve().parent
release = here.parents[1]
sketch = release / 'Tab5OS_Arduino_v0.8.28_Ecriture'
build = Path('C:/workspace/tab5-memory-0828')
build.mkdir(parents=True, exist_ok=True)
database_path = Path(sys.argv[1] if len(sys.argv) > 1 else 'C:/workspace/tab5-build-0827-clang/compile_commands.json')
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
    argument = re.sub(r'Tab5OS_Ecriture_v0\.8\.\d+', release.name, argument)
    argument = re.sub(r'Tab5OS_Arduino_v0\.8\.\d+_Ecriture', sketch.name, argument)
    arguments.append(argument)

units = ['Tab5Memory.cpp', 'Tab5ResourceManager.cpp', 'Tab5Diagnostics.cpp',
         'Tab5AppPanel.cpp', 'Tab5Tools.cpp', 'Tab5FileShare.cpp',
         'Tab5NetworkTools.cpp', 'Tab5NetworkHttp.cpp', 'Tab5PrinterDiscovery.cpp',
         'Tab5Sdr.cpp', 'Tab5SdrUi.cpp', 'src/rtl_sdr/usb_adapter.cpp']
source_names = units + ['Tab5Memory.h', 'Tab5ResourceManager.h', 'Tab5Task.h',
                        'Tab5Diagnostics.h', 'Tab5AppPanel.h', 'Tab5ConsoleBuffer.h',
                        'Tab5CredentialUi.inc', 'Tab5Sdr.h', 'Tab5SdrDsp.h',
                        'Tab5SdrModel.h', 'Tab5SdrUi.h', 'src/rtl_sdr/libusb.h',
                        'Tab5BatteryGuard.h', 'Tab5KeyStore.h']
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
before = {name: sha(sketch / name) for name in source_names}
response_files = {argument[1:]: sha(Path(argument[1:])) for argument in arguments if argument.startswith('@')}
compiler = Path(arguments[0])
if not compiler.is_file():
    compiler = Path(arguments[0] + '.exe')
assert compiler.is_file(), arguments[0]
assert '--target=riscv32-esp-unknown-elf' in arguments
assert '-DARDUINO_M5STACK_TAB5' in arguments
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

stable = before == {name: sha(sketch / name) for name in source_names}
flags_stable = response_files == {path: sha(Path(path)) for path in response_files}
report = dict(version='0.8.28', checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              passed=stable and flags_stable and all(row['exitCode'] == 0 for row in results),
              sourceStableDuringTests=stable, sdkFlagsStableDuringTests=flags_stable,
              method='Compile current .28 translation units using the installed ESP32-P4 Clang toolchain and SDK flags from the .27 compile database; -fstack-usage per-function frames. No linking or flashing; a largest single frame is NOT a full call-chain bound.',
              physicalDeviceTested=False, compileDatabase=str(database_path),
              compileDatabaseSha256=sha(database_path), compilerSha256=sha(compiler), sdkResponseFiles=response_files,
              results=results, sources=before,
              tests={Path(__file__).name: sha(Path(__file__))})
(release / 'tools/verification/memory-target-stack-0828.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
