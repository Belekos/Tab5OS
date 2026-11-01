#!/usr/bin/env python3
"""Audit current Explorer literals against delivered 0.8.37 and all 14 packs.

The old localization report is not relabelled. This audit compares current
Explorer/main strings to the authenticated baseline, verifies every added
user-facing literal, and checks preservation of all existing pack entries and
metadata. Changed/new files are included in full. Historical engine tests are
outside this audit; their byte preservation is checked separately.
"""
from pathlib import Path
import argparse
import ast
import hashlib
import json
import re
import sys
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SKETCH = ROOT / 'Tab5OS_Arduino_v0.8.38_Ecriture'
BASELINE_SHA256 = 'f8ec50004a841bc6d7f3502de240dd4eece58262f5360e00447e3ab47cb27a24'
BASELINE_PREFIX = 'Tab5OS_Ecriture_v0.8.37/Tab5OS_Arduino_v0.8.37_Ecriture/'
sys.path.insert(0, str(ROOT / 'tools'))
from audit_languages import no_duplicate_pairs, signature

TECHNICAL = {
    '0.8.38': 'Firmware version identifier',
    '/USB': 'Logical USB root path',
    '/USB/': 'Logical USB path prefix',
    'Git': 'Product/protocol name, identical in all languages',
    'SD': 'Storage technology name, identical in all languages',
    '/MicroPython/libraries.py': 'English filesystem path for the shipped library utility',
    'New folder': 'English default folder name required independently of UI language',
    ' - copy': 'English duplicate-name suffix required independently of UI language',
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def literals(text):
    values = set()
    for token in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:[^"\\]|\\.)*"', text):
        if token.group().startswith('"'):
            values.add(ast.literal_eval(token.group()))
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    args = parser.parse_args()
    assert sha(args.baseline) == BASELINE_SHA256, 'Not the delivered 0.8.37 ZIP'
    sources = sorted([SKETCH / (SKETCH.name + '.ino'), *SKETCH.glob('Tab5Explorer*'), *SKETCH.glob('Tab5FolderPreview*')])
    hashes = {str(path.relative_to(ROOT)): sha(path) for path in sources}
    keys = set()
    source_keys = {}
    excluded = {}
    packs = {}
    with zipfile.ZipFile(args.baseline) as archive:
        assert archive.testzip() is None, 'Baseline ZIP CRC mismatch'
        for path in sources:
            old_name = path.name.replace('0.8.38', '0.8.37')
            try:
                previous = literals(archive.read(BASELINE_PREFIX + old_name).decode('utf-8'))
            except KeyError:
                previous = set()
            added = literals(path.read_text(encoding='utf-8')) - previous
            source_keys[path.name] = []
            for value in sorted(added):
                if not value or not re.search('[A-Za-zÀ-ÿ]', value):
                    continue
                reason = TECHNICAL.get(value)
                if value.endswith(('.h', '.inc')):
                    reason = 'Source include filename'
                if reason:
                    excluded[value] = reason
                    continue
                keys.add(value)
                source_keys[path.name].append(value)
        for path in sorted((SKETCH / 'sdcard/System/languages').glob('*.json')):
            relative = path.relative_to(SKETCH).as_posix()
            current = json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=no_duplicate_pairs)
            original = json.loads(archive.read(BASELINE_PREFIX + relative), object_pairs_hook=no_duplicate_pairs)
            assert {k: v for k, v in current.items() if k != 'translations'} == {
                k: v for k, v in original.items() if k != 'translations'}, (path.name, 'metadata changed')
            rows = current['translations']
            assert all(rows.get(k) == v for k, v in original['translations'].items()), (
                path.name, 'existing translation changed')
            assert not keys - rows.keys(), (path.name, 'untranslated keys', sorted(keys - rows.keys()))
            for key in keys:
                assert isinstance(rows[key], str) and rows[key], (path.name, key, 'empty')
                assert signature(key) == signature(rows[key]), (path.name, key, 'format')
                assert key.count('\n') == rows[key].count('\n'), (path.name, key, 'line breaks')
            packs[path.stem] = {
                'entries': len(rows), 'newKeys': len(set(rows) - set(original['translations'])),
                'bytes': path.stat().st_size, 'sha256': sha(path),
                'byteIdenticalToBaseline': path.read_bytes() == archive.read(BASELINE_PREFIX + relative),
            }
            hashes[str(path.relative_to(ROOT))] = sha(path)
    assert len(packs) == 14, 'Expected all 14 languages'
    assert {str(path.relative_to(ROOT)): sha(path) for path in sources} == {
        str(path.relative_to(ROOT)): hashes[str(path.relative_to(ROOT))] for path in sources}, 'Sources changed during audit'
    hashes[str(Path(__file__).relative_to(ROOT))] = sha(Path(__file__))
    hashes['tools/audit_languages.py'] = sha(ROOT / 'tools/audit_languages.py')
    report = {
        'version': '0.8.38', 'passed': True, 'method': __doc__,
        'baselineZipSha256': BASELINE_SHA256,
        'baselineTranslationsAndMetadataPreserved': True,
        'packs': packs, 'sourceHashes': hashes,
        'sourceKeyCoverage': {
            'passed': True, 'translatedKeysInChangedSources': len(keys),
            'missingKeys': [], 'keysBySource': source_keys, 'technicalExclusions': excluded,
            'method': 'Current Explorer/main literal additions against authenticated delivered 0.8.37; all 14 packs checked without rewriting existing translations.',
        },
        'limitations': ['Static source and pack verification; no physical device or live language-switch test.'],
    }
    target = ROOT / 'tools/verification/drawing-localization-0838.json'
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'passed': True, 'addedSourceKeys': len(keys), 'packs': len(packs),
                      'unchangedPacks': sum(row['byteIdenticalToBaseline'] for row in packs.values()),
                      'report': str(target)}))


if __name__ == '__main__':
    main()
