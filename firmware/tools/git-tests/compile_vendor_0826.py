"""Compile the shipped C sources with a supplied ESP32-P4 response file."""
from pathlib import Path
import argparse, subprocess, json, concurrent.futures, hashlib

here = Path(__file__).resolve().parent
release = here.parent.parent
sketch = release / 'Tab5OS_Arduino_v0.8.26_Ecriture'
vendor = sketch / 'src/tab5git'
parser = argparse.ArgumentParser()
parser.add_argument('--compiler', required=True)
parser.add_argument('--response', type=Path, required=True)
parser.add_argument('--build', type=Path, required=True)
args = parser.parse_args()
args.build.mkdir(parents=True, exist_ok=True)
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
inputs = sorted([p for p in vendor.rglob('*') if p.is_file() and p.suffix in ('.c', '.h', '.inc')])
snapshot = {p.relative_to(release).as_posix(): digest(p) for p in inputs}

def compile_one(p):
    target = args.build / p.relative_to(vendor).with_suffix('.o')
    target.parent.mkdir(parents=True, exist_ok=True)
    command = [args.compiler, '@' + str(args.response), '-std=gnu17', '-c', str(p), '-o', str(target)]
    result = subprocess.run(command, capture_output=True, text=True)
    return {'file': p.relative_to(sketch).as_posix(), 'sha256': digest(p),
            'exit': result.returncode, 'diagnostic': (result.stdout + result.stderr)[-5000:]}

with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
    rows = list(pool.map(compile_one, sorted(vendor.rglob('*.c'))))
stable = snapshot == {p.relative_to(release).as_posix(): digest(p) for p in inputs}
report = {'version': '0.8.26', 'passed': stable and all(not r['exit'] for r in rows), 'units': len(rows),
          'compiler': args.compiler, 'target': 'ESP32-P4 Arduino SDK 3.3.9 actual native C flags; relative include vendored source',
          'physicalDeviceTested': False, 'sourcesStableDuringTest': stable, 'results': rows,
          'inputs': snapshot, 'runner': {'path': here.joinpath('compile_vendor_0826.py').relative_to(release).as_posix(), 'sha256': digest(Path(__file__))},
          'responseSha256': digest(args.response), 'responseFlags': args.response.read_text()}
(release / 'tools/verification/git-vendor-compile-0826.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({'passed': report['passed'], 'units': len(rows)}))
raise SystemExit(0 if report['passed'] else 1)
