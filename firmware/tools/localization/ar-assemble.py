#!/usr/bin/env python3
"""Assemble reviewed Arabic translations without fallback to another language."""
from pathlib import Path
import importlib.util
import json
import re

HERE = Path(__file__).resolve().parent
RELEASE = HERE.parent.parent
REFERENCE = json.loads((HERE / 'reference-complete-0817.json').read_text(encoding='utf-8-sig'))
shell_delta = HERE / 'delta-shell-0817.json'
if shell_delta.exists():
    known = {row['index']: row['source'] for row in REFERENCE}
    for row in json.loads(shell_delta.read_text(encoding='utf-8-sig')):
        if row['index'] in known:
            if known[row['index']] != row['source']:
                raise ValueError(f"Conflicting reference index {row['index']}")
        else:
            REFERENCE.append(row)
spec = importlib.util.spec_from_file_location('language_audit', RELEASE / 'tools/audit_languages.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)
translations = {}
fragments = []
for path in sorted(HERE.glob('ar-*.json')):
    if not re.fullmatch(r'ar-\d{4}-\d{4}\.json', path.name):
        continue
    rows = json.loads(path.read_text(encoding='utf-8-sig'), object_pairs_hook=audit.no_duplicate_pairs)
    fragments.append(path.name)
    for index, value in rows.items():
        if index in translations and translations[index] != value:
            raise ValueError(f'Conflicting translation {index}: {path.name}')
        translations[index] = value
expected = {str(row['index']) for row in REFERENCE}
missing = sorted(expected - translations.keys(), key=int)
extra = sorted(translations.keys() - expected, key=int)
if missing or extra:
    raise ValueError(f'Missing: {missing}; extra: {extra}')
errors = []
same_english = []
for row in REFERENCE:
    index = str(row['index'])
    value = translations[index]
    if not isinstance(value, str) or not value:
        errors.append({'index': index, 'error': 'empty translation'})
        continue
    if audit.signature(row['source']) != audit.signature(value):
        errors.append({'index': index, 'error': 'printf mismatch'})
    if row['source'].count('\n') != value.count('\n'):
        errors.append({'index': index, 'error': 'newline mismatch'})
    if any(0xFB50 <= ord(c) <= 0xFDFF or 0xFE70 <= ord(c) <= 0xFEFF for c in value):
        errors.append({'index': index, 'error': 'Arabic presentation form'})
    if any(c in value for c in '\u200e\u200f\u202a\u202b\u202c\u202d\u202e\u2066\u2067\u2068\u2069'):
        errors.append({'index': index, 'error': 'manually inserted bidi control'})
    if value == row['english']:
        same_english.append({'index': row['index'], 'value': value})
if errors:
    raise ValueError(json.dumps(errors, ensure_ascii=False))
pack = {
    'version': 3,
    'id': 'ar',
    'name': 'العربية',
    'translations': {row['source']: translations[str(row['index'])] for row in REFERENCE},
}
target = RELEASE / 'Tab5OS_Arduino_v0.8.18_Ecriture/sdcard/System/languages/ar.json'
serialized = json.dumps(pack, ensure_ascii=False, indent=2) + '\n'
size = len(serialized.encode('utf-8'))
if size > 512 * 1024:
    raise ValueError(f'Pack exceeds 512 KiB: {size} bytes')
target.write_bytes(serialized.encode('utf-8'))
report = {
    'entries': len(pack['translations']),
    'coveragePercent': 100,
    'formatErrors': [],
    'newlineErrors': [],
    'presentationForms': [],
    'bidiControls': [],
    'bytes': size,
    'unchangedTechnicalLabels': same_english,
    'fragments': fragments,
}
(HERE / 'ar-translation-verification.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report, ensure_ascii=False, indent=2))
