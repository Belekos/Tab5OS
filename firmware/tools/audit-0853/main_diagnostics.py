"""Compile the current generated main with safety diagnostics, without inventing a baseline run."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from release_config import VERSION, TAG, SKETCH
sys.path.insert(0, str(ROOT / 'tools/build_linux'))
from compile_linux import manifest

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment', required=True, type=Path)
    args = parser.parse_args()
    env = args.environment.resolve()
    build = env / 'build'
    qa = env / 'qa'
    sketch = ROOT / SKETCH
    result = json.loads((qa / 'build-result.json').read_text())
    assert result['exitCode'] == 0 and result['sourcesUnchanged']
    before = manifest(sketch)
    assert before == json.loads((qa / 'source-manifest-end.json').read_text())['files']
    name = SKETCH + '.ino.cpp'
    database = build / 'compile_commands.json'
    entry = next(row for row in json.loads(database.read_text())
                 if Path(row['file']).name == name)
    source = build / 'sketch' / name
    output = env / 'main-diagnostics'
    output.mkdir(exist_ok=True)
    flags = ['-Wall', '-Wextra', '-Werror=return-type', '-Werror=uninitialized',
             '-Werror=maybe-uninitialized', '-Werror=use-after-free']
    command = [str(source) if value == entry['file'] else value
               for value in entry['arguments'] if value != '-w']
    assert str(source) in command and str(qa) in ' '.join(command)
    command.extend(flags)
    command[command.index('-o') + 1] = str(output / (name + '.o'))
    execution = subprocess.run(command, cwd=entry['directory'], capture_output=True, text=True)
    logs = ROOT / f'tools/verification/main-diagnostics-{TAG}'
    logs.mkdir(exist_ok=True)
    log = logs / 'compile.log'
    text = execution.stdout + execution.stderr
    log.write_text(text)
    assert before == manifest(sketch), 'Sources changed during diagnostics'
    hashes = {f'{SKETCH}/{row["path"]}': row['sha256'] for row in before
              if '/' not in row['path'] and Path(row['path']).suffix in {'.ino', '.h', '.inc'}}
    report = {
        'version': VERSION, 'passed': execution.returncode == 0,
        'physicalDeviceTested': False,
        'scope': 'Current generated Arduino main and its included headers compiled with safety diagnostic errors enabled; no baseline comparison.',
        'exitCode': execution.returncode,
        'flags': flags, 'removedFlag': '-w',
        'warningCount': sum('warning:' in line for line in text.splitlines()),
        'errorCount': sum('error:' in line for line in text.splitlines()),
        'generatedMainSha256': sha(source),
        'compilationDatabase': str(database), 'compilationDatabaseSha256': sha(database),
        'command': command,
        'sourceHashes': hashes,
        'testSourceHashes': {Path(__file__).relative_to(ROOT).as_posix(): sha(Path(__file__))},
        'evidenceHashes': {log.relative_to(ROOT).as_posix(): sha(log)},
        'limitations': ['No physical device execution.',
                       'Warnings are retained in full; this does not prove absence of runtime lifetime or concurrency defects.',
                       'No authenticated generated .52 main was available for a diagnostic baseline comparison.']
    }
    (ROOT / f'tools/verification/main-diagnostics-{TAG}.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({key: report[key] for key in ('passed', 'exitCode', 'warningCount', 'errorCount')}))
    if execution.returncode:
        print(text[-12000:])
        raise SystemExit(execution.returncode)

if __name__ == '__main__':
    main()
