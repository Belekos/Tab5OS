"""Check the actual network/terminal sources with the release ESP32-P4 flags."""
import argparse, hashlib, json, pathlib, subprocess

p = argparse.ArgumentParser()
p.add_argument('--database', type=pathlib.Path, required=True)
p.add_argument('--sketch', type=pathlib.Path, required=True)
p.add_argument('--output', type=pathlib.Path, required=True)
a = p.parse_args()
entries = json.loads(a.database.read_text(encoding='utf-8'))
template = next(e for e in entries if e['file'].endswith('Tab5Terminal.cpp'))
args = template['arguments']
root = a.sketch.resolve()
results = []
for filename in ['Tab5NetworkTools.cpp', 'Tab5Terminal.cpp', 'Tab5Shell.cpp']:
    command = []
    skip = False
    for argument in args:
        if skip:
            skip = False
            continue
        if argument == '-o':
            skip = True
            continue
        if argument in ['-c', '-MMD', '-fstack-usage'] or argument == template['file']:
            continue
        if argument.startswith('-I') and 'Tab5OS_Arduino_v0.8.17_Ecriture' in argument:
            argument = '-I' + str(root)
        command.append(argument)
    command += ['-fsyntax-only', str(root / filename)]
    result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8')
    results.append({'file': filename, 'sha256': hashlib.sha256((root / filename).read_bytes()).hexdigest(),
                    'exitCode': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
    print(filename, result.returncode, result.stderr)
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding='utf-8')
raise SystemExit(any(result['exitCode'] != 0 for result in results))
