"""Compile the vendored SSH code against an existing P4 compile database.

No download, linking or device flashing is performed. Run from any directory:
python compile_target.py --compile-db path/to/compile_commands.json --output path/to/objects
"""
from pathlib import Path
import argparse, concurrent.futures, hashlib, json, subprocess, re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compile-db', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
release = Path(__file__).resolve().parents[2]
sketch = release / 'Tab5OS_Arduino_v0.8.22_Ecriture'
db = json.loads(args.compile_db.read_text(encoding='utf-8'))
template = next(row for row in db if Path(row['file']).name == 'Tab5Tools.cpp')
old_sketch = Path(template['file']).parent
base = []
skip = False
for arg in template['arguments']:
    if skip:
        skip = False
        continue
    if arg == '-o':
        skip = True
        continue
    if arg in ('-c', '-MMD', template['file'], '-fno-rtti', '-fno-exceptions') or arg.startswith('-std='):
        continue
    arg = arg.replace(str(old_sketch), str(sketch)).replace(old_sketch.as_posix(), sketch.as_posix())
    arg = re.sub(r'Tab5OS_Ecriture_v\d+\.\d+\.\d+', release.name, arg)
    arg = re.sub(r'Tab5OS_Arduino_v\d+\.\d+\.\d+_Ecriture', sketch.name, arg)
    base.append(arg)
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
paths = sorted((sketch / 'src/tab5ssh').glob('*.c')) + [sketch / 'Tab5Ssh.cpp']

def compile_one(path):
    cpp = path.suffix == '.cpp'
    command = base + ['-x', 'c++' if cpp else 'c', '-std=gnu++17' if cpp else '-std=gnu17', '-fstack-usage', '-c', str(path), '-o', str(out / (path.stem + '.o'))]
    result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', errors='replace')
    return {'file': path.relative_to(sketch).as_posix(), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'exitCode': result.returncode, 'output': result.stdout + result.stderr}

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    rows = list(pool.map(compile_one, paths))
frames = []
for path in paths:
    usage = out / (path.stem + '.su')
    if usage.exists():
        for line in usage.read_text(encoding='utf-8').splitlines():
            fields = line.rsplit('\t', 2)
            if len(fields) == 3:
                frames.append({'function': fields[0].replace(str(sketch), '').replace(sketch.as_posix(), ''), 'bytes': int(fields[1]), 'kind': fields[2]})
report = {
    'version': '0.8.22',
    'method': 'Actual ESP32-P4 object compilation against installed SDK, -fstack-usage; no final link or physical-device claim.',
    'compileDatabaseSha256': hashlib.sha256(args.compile_db.read_bytes()).hexdigest(),
    'compiler': base[0],
    'physicalDeviceTested': False,
    'passed': all(row['exitCode'] == 0 for row in rows),
    'files': rows,
    'inputs': {path.relative_to(sketch).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted((sketch / 'src/tab5ssh').glob('*')) + [sketch / name for name in ('Tab5Ssh.h', 'Tab5SshPolicy.h', 'Tab5TerminalPath.h')] if path.is_file()},
    'tests': {'compile_target.py': hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},
    'largestStaticFrames': sorted(frames, key=lambda row: row['bytes'], reverse=True)[:30],
    'stackLimitation': 'Individual compiler frames are not a measured call-chain maximum. The large bcrypt/OpenSSH-key and known-host file functions are not used by this command. Physical stack watermarks remain to be measured.'
}
for index, arg in enumerate(base[:-1]):
    if arg == '-iprefix':
        header = Path(base[index + 1]) / 'mbedtls/mbedtls/include/mbedtls/build_info.h'
        if header.exists():
            report['targetMbedtls'] = {'version': re.search(r'#define MBEDTLS_VERSION_STRING\s+"([^"]+)"', header.read_text()).group(1), 'buildInfoSha256': hashlib.sha256(header.read_bytes()).hexdigest(), 'path': str(header)}
(release / 'tools/verification/ssh-target-0822.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
for row in rows:
    print(row['file'], row['exitCode'])
    if row['exitCode']:
        print(row['output'][:7000])
raise SystemExit(not report['passed'])
