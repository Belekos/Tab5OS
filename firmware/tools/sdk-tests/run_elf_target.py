"""Compile ABI5/ELF units for the actual ESP32-P4 and report stack frames."""
from pathlib import Path
import datetime
import hashlib
import json
import subprocess
import sys

here = Path(__file__).resolve().parent
release = here.parents[1]
sketch = release / 'Tab5OS_Arduino_v0.8.23_Ecriture'
build = Path('C:/workspace/tab5-elf-target-0823')
build.mkdir(parents=True, exist_ok=True)
database_path = Path(sys.argv[1] if len(sys.argv) > 1 else 'C:/workspace/tab5-build-0822-clang/compile_commands.json')
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
    arguments.append(argument.replace('Tab5OS_Ecriture_v0.8.22', 'Tab5OS_Ecriture_v0.8.23').replace('Tab5OS_Arduino_v0.8.22', 'Tab5OS_Arduino_v0.8.23'))

units = ['Tab5Executable.cpp', 'Tab5NativeView.cpp', 'Tab5AppPanel.cpp', 'Tab5ElfValidate.cpp', 'Tab5Tools.cpp']
names = units + ['Tab5AppApi.h', 'Tab5AppPanel.h', 'Tab5ConsoleBuffer.h', 'Tab5Task.h']
before = {n: hashlib.sha256((sketch / n).read_bytes()).hexdigest() for n in names}
results = []
for unit in units:
    output = build / (unit + '.o')
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
    print(unit, result.returncode, 'largest frame', row['frames'][0]['bytes'] if row['frames'] else '-', flush=True)

after = {n: hashlib.sha256((sketch / n).read_bytes()).hexdigest() for n in names}
report = dict(version='0.8.23', checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),
              passed=all(row['exitCode'] == 0 for row in results) and before == after,
              sourceStableDuringTests=before == after,
              method='Compile current ELF/ABI5 production units using installed ESP32-P4 Clang, SDK headers and flags from the previous complete build. Reports actual target single-function stack frames. No linking or flashing; largest frame is not the bound for a full call chain.',
              physicalDeviceTested=False, compileDatabase=str(database_path),
              compileDatabaseSha256=hashlib.sha256(database_path.read_bytes()).hexdigest(),
              results=results, sources=before,
              tests={Path(__file__).name: hashlib.sha256(Path(__file__).read_bytes()).hexdigest()})
(release / 'tools/verification/elf-target-stack-0823.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
raise SystemExit(0 if report['passed'] else 1)
