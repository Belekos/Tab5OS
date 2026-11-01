"""Candidate C++ UI literals changed since .24; review before translation."""
from pathlib import Path
import ast
import collections
import json
import re

HERE = Path(__file__).resolve().parent
RELEASE = HERE.parents[1]
SKETCH = next(RELEASE.glob('Tab5OS_Arduino_*_Ecriture'))
PREVIOUS = RELEASE.parent/'Tab5OS_Ecriture_v0.8.24'/'Tab5OS_Arduino_v0.8.24_Ecriture'
reference = json.loads((SKETCH/'sdcard/System/languages/en-US.json').read_text(encoding='utf-8'))['translations']
literal = re.compile(r'"(?:[^"\\\r\n]|\\.)*"')


def literals(path):
    if not path.exists():
        return {}
    result = collections.defaultdict(list)
    for number, line in enumerate(path.read_text(encoding='utf-8-sig').splitlines(), 1):
        if line.strip().startswith(('#include', '//', '*')):
            continue
        for match in literal.finditer(line):
            try:
                value = ast.literal_eval(match.group())
            except (ValueError, SyntaxError):
                continue
            result[value].append(number)
    return result


found = collections.defaultdict(list)
for path in sorted(SKETCH.iterdir()):
    if path.suffix not in ('.h', '.cpp', '.ino', '.inc') or path.name.startswith(('Tab5English', 'Tab5SystemResourcesData')):
        continue
    old = PREVIOUS / path.name.replace('0.8.25', '0.8.24')
    previous = literals(old)
    for key, lines in literals(path).items():
        if key in previous or key in reference or not re.search(r'[A-Za-zÀ-ž]', key):
            continue
        found[key].append({'file': path.name, 'lines': lines})
(HERE/'candidates-0825.json').write_text(json.dumps(found, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
for n, (key, locations) in enumerate(found.items(), 1):
    print(n, repr(key), ','.join(location['file'] for location in locations))
