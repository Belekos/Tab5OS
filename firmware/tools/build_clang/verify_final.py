"""Verify the completed build and confirm its artifacts stay unchanged."""
import concurrent.futures
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys

build = Path(r'C:\workspace\tab5-build-0828-clang')
qa = Path(r'C:\workspace\tab5-qa-0828-clang')
helpers = Path(__file__).resolve().parent

def now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()

def snapshot():
    rows = []
    for p in sorted(build.rglob('*')):
        if not p.is_file() or p.suffix not in ['.o', '.a', '.bin', '.elf', '.map']:
            continue
        stat = p.stat()
        row = {'path': p.relative_to(build).as_posix(), 'bytes': stat.st_size,
               'mtimeNs': stat.st_mtime_ns}
        if p.suffix in ['.bin', '.elf', '.map']:
            row['sha256'] = hashlib.sha256(p.read_bytes()).hexdigest()
        rows.append(row)
    return rows

result = json.loads((qa / 'build-result.json').read_text())
if result.get('exitCode') != 0:
    raise RuntimeError('Wait for successful completion of the build first.')
started = now()
before = snapshot()

def check(script):
    result = subprocess.run([sys.executable, '-X', 'utf8', str(helpers / script)], capture_output=True, text=True)
    (qa / (Path(script).stem + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
    return {'script': script, 'exitCode': result.returncode, 'output': result.stdout + result.stderr}

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    checks = list(pool.map(check, ['verify_build.py', 'verify_archives.py']))
after = snapshot()
report = {'startedAt': started, 'finishedAt': now(), 'filesChecked': len(before),
          'artifactsStable': before == after, 'before': before, 'after': after,
          'checks': [{'script': c['script'], 'exitCode': c['exitCode']} for c in checks]}
report['passed'] = report['artifactsStable'] and all(c['exitCode'] == 0 for c in checks)
(qa / 'artifact-stability.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
for check in checks:
    print(check['script'], check['exitCode'])
    print(check['output'])
print(json.dumps({key: report[key] for key in ['startedAt', 'finishedAt', 'filesChecked', 'artifactsStable', 'passed']}, indent=2))
raise SystemExit(0 if report['passed'] else 1)
