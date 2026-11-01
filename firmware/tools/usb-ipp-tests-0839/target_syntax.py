"""Check USB production sources using an existing pinned P4 compile database.

Usage: python3 tools/usb-ipp-tests-0839/target_syntax.py /path/to/compile_commands.json
This checks source syntax and SDK types; it is not a firmware link or device test.
"""
import hashlib
import json
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[2]
sketch = root / 'Tab5OS_Arduino_v0.8.39_Ecriture'
database = pathlib.Path(sys.argv[1]).resolve()
entries = json.loads(database.read_text())
out = root / 'tools/verification/usb-target-0839'
out.mkdir(parents=True, exist_ok=True)
names = ['Tab5UsbStorage.cpp', 'src/usb_msc/tab5_msc_format.c',
         'Tab5UsbPrinter.cpp', 'Tab5Print.cpp', 'Tab5PrinterDiscovery.cpp', 'WriterUsb.cpp']
headers = ['Tab5UsbStorage.h', 'Tab5UsbFormatPolicy.h', 'src/usb_msc/tab5_msc_format.h',
           'Tab5UsbPrinter.h', 'Tab5UsbIppProtocol.h']


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


sources = {str((sketch / n).relative_to(root)): sha(sketch / n) for n in names + headers}
results = []
for name in names:
    suffix = '/src/usb_msc/msc_host_vfs.c' if name.endswith('.c') else '/Tab5Print.cpp'
    template = next(e for e in entries if e['file'].endswith(suffix))
    original = template['arguments']
    args = []
    skip = False
    for arg in original:
        if skip:
            skip = False
            continue
        if arg in ('-o', '-MF', '-MT', '-MQ'):
            skip = True
            continue
        if arg in ('-c', '-MMD', '-MD', '-MP', '-fstack-usage'):
            continue
        if arg == template['file']:
            args.append(str(sketch / name))
        elif arg.startswith('-I') and pathlib.Path(arg[2:]).name.startswith('Tab5OS_Arduino_v'):
            args.append('-I' + str(sketch))
        else:
            args.append(arg)
    args.append('-fsyntax-only')
    process = subprocess.run(args, cwd=root, capture_output=True, text=True)
    log = out / (pathlib.Path(name).name + '.log')
    log.write_text(process.stdout + process.stderr)
    results.append({'source': name, 'exitCode': process.returncode, 'arguments': args})
    print(name + ': ' + ('PASS' if process.returncode == 0 else 'FAIL'))
command_log = out / 'commands.json'
command_log.write_text(json.dumps(results, indent=2) + '\n')
report = {
    'version': '0.8.39',
    'passed': all(r['exitCode'] == 0 for r in results),
    'sourceStableDuringTests': all(sha(root / p) == h for p, h in sources.items()),
    'sourceHashes': sources,
    'testSourceHashes': {str(pathlib.Path(__file__).resolve().relative_to(root)): sha(pathlib.Path(__file__))},
    'evidenceHashes': {str(p.relative_to(root)): sha(p) for p in out.iterdir() if p.is_file()},
    'compileDatabase': str(database),
    'compileDatabaseSha256': sha(database),
    'method': 'Pinned ESP32-P4 compiler and SDK includes from the supplied compilation database; six production source units checked with -fsyntax-only. No link and no hardware execution.',
}
(out.parent / 'usb-target-0839.json').write_text(json.dumps(report, indent=2) + '\n')
sys.exit(0 if report['passed'] and report['sourceStableDuringTests'] else 1)
