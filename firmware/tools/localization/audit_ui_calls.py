"""Check literal locale calls plus the separately inventoried shell help/errors."""
import ast
import json
from pathlib import Path
import re
import hashlib
from datetime import datetime,timezone
from ui_producer_keys import inventory,invariant_key,arguments,literals,code_only

release = Path(__file__).resolve().parents[2]
sketch = next(release.glob('Tab5OS_Arduino_*_Ecriture'))
version = re.search(r'_v(\d+\.\d+\.\d+)_', sketch.name).group(1)
suffix = version.replace('.', '')
reference = json.loads((sketch / 'sdcard/System/languages/en-US.json').read_text(encoding='utf-8'))['translations']
missing = {}
count = 0
producer_rows=[]
for path in sketch.iterdir():
    if path.suffix not in {'.cpp', '.h', '.ino', '.inc'} or path.name.startswith('Tab5English'):
        continue
    text = path.read_text(encoding='utf-8')
    producer_rows.extend(inventory(path))
    for call in re.finditer(r'\b(?:T|tr)\s*\(',code_only(text)):
        parts,_=arguments(text,call.end())
        if parts and not parts[0].lstrip().startswith('"'):
            for key in literals(parts[0]):
                producer_rows.append({'key':key,'location':path.name+':'+str(text.count('\n',0,call.start())+1),'producer':'conditional translation expression'})
    for match in re.finditer(r'\b(?:T|tr|copyText)\s*\(\s*((?:"(?:\\.|[^"\\])*"\s*)+)', text):
        try:
            source = ''.join(json.loads(part) for part in re.findall(r'"(?:\\.|[^"\\])*"', match.group(1)))
        except ValueError:
            continue
        if not source:
            continue
        count += 1
        if source not in reference:
            missing.setdefault(source, []).append(path.name + ':' + str(text.count('\n', 0, match.start()) + 1))
for row in producer_rows:
    if row['key'] not in reference and not invariant_key(row['key']):
        missing.setdefault(row['key'],[]).append(row['location']+' ['+row['producer']+']')
for path in (sketch / 'sdcard/MicroPython').rglob('*.py'):
    tree = ast.parse(path.read_text(encoding='utf-8-sig'))
    for node in ast.walk(tree):
        if not isinstance(node, ast.Call) or not node.args:
            continue
        function = node.func.id if isinstance(node.func, ast.Name) else node.func.attr if isinstance(node.func, ast.Attribute) else ''
        if function not in {'tr', '_tr', 'T'} or not isinstance(node.args[0], ast.Constant) or not isinstance(node.args[0].value, str):
            continue
        source = node.args[0].value
        if not source:
            continue
        count += 1
        if source not in reference:
            missing.setdefault(source, []).append(path.relative_to(sketch).as_posix() + ':' + str(node.lineno))
shell = json.loads((Path(__file__).parent / 'delta-shell-0817.json').read_text(encoding='utf-8'))
for row in shell:
    if row['source'] not in reference:
        missing.setdefault(row['source'], []).append('shell inventory')
help_entries=[]
for row in (sketch/'Tab5ShellHelp.h').read_text(encoding='utf8').splitlines():
    row=row.strip().rstrip(',')
    if row.startswith('{"'):help_entries.append(json.loads('['+row[1:-1]+']'))
for entry in help_entries:
    if entry[2] not in reference:missing.setdefault(entry[2],[]).append('help description: '+entry[0])
new_keys=set()
for name in ['reference-delta-0819.json','reference-late-0819.json','reference-final-0819.json']:
    new_keys.update(json.loads((Path(__file__).parent/name).read_text(encoding='utf8')))
for key in new_keys:
    if key not in reference:missing.setdefault(key,[]).append('0.8.19 UI/network/help inventory')
current_inventory=Path(__file__).parent/('reference-delta-'+suffix+'.json')
final_inventory=Path(__file__).parent/('reference-final-delta-'+suffix+'.json')
if final_inventory.exists():current_inventory=final_inventory
current_keys=json.loads(current_inventory.read_text(encoding='utf8')) if current_inventory.exists() else []
for key in current_keys:
    if key not in reference:missing.setdefault(key,[]).append(version+' UI inventory')
embedded=[]
for match in re.finditer(r'\{\s*("(?:\\.|[^"\\])*")\s*,\s*("(?:\\.|[^"\\])*")\s*\}',(sketch/'Tab5Help.cpp').read_text(encoding='utf8')):
    title,body=map(json.loads,match.groups());embedded.append(title)
    for key in [title,body]:
        if key not in reference:missing.setdefault(key,[]).append('embedded help: '+title)
report = {'referenceEntries': len(reference), 'literalTranslationCallsites': count, 'shellInventoryEntries': len(shell), 'helpPages':len(help_entries),'embeddedHelpPages':len(embedded),'newUiNetworkHelpKeys':len(new_keys), 'missingKeys': missing, 'limitations': 'Static literal calls, embedded help pages, terminal help registry and explicit network/error inventory; runtime document contents, user filenames, server responses, script exceptions and external ELF framebuffer text are not translation keys.', 'passed': not missing}
report.update({'release':version,'checkedAt':datetime.now(timezone.utc).isoformat(),'currentReleaseKeys':len(current_keys),'sourceHashes':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sketch.iterdir() if p.suffix in {'.cpp','.h','.ino','.inc'} and not p.name.startswith('Tab5English')}})
report.update({'indirectUiProducerCallsites':len(producer_rows),'indirectUiProducerKeys':sorted({row['key'] for row in producer_rows}),
 'invariantUiProducerKeys':sorted({row['key'] for row in producer_rows if invariant_key(row['key'])}),
 'testSources':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__),Path(__file__).with_name('ui_producer_keys.py')]}})
target = release / ('tools/verification/ui-locale-calls-'+suffix+'.json')
target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report, ensure_ascii=False, indent=2))
raise SystemExit(1 if missing else 0)
