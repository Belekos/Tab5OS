#!/usr/bin/env python3
"""Check reviewed .53 UI additions and preservation of the delivered .52 packs."""
from pathlib import Path
import argparse
import ast
import hashlib
import json
import re
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from release_config import VERSION, TAG, SKETCH, BASELINE_VERSION, BASELINE_ARCHIVE_SHA256
from audit_languages import signature, no_duplicate_pairs

TECHNICAL = {
    '0.8.53': 'Firmware version identifier',
    '@draw': 'Vector icon identifier, not a user-facing label',
    'apps-v2': 'Versioned NVS preference key',
    **{key: 'Stable built-in application catalogue identifier' for key in
       ('home', 'files', 'editor', 'reader', 'audio', 'map', 'settings', 'image-editor')},
    'sidebar v1 on-disk layout': 'Compile-time persistence layout assertion',
    'sidebar v2 on-disk layout': 'Compile-time persistence layout assertion',
}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def literals(text):
    pattern = r'//[^\n]*|/\*[\s\S]*?\*/|R"(?P<delimiter>[^\s()\\]{0,16})\((?P<raw>[\s\S]*?)\)(?P=delimiter)"|\'(?:[^\'\\\n]|\\.)*\'|"(?:[^"\\]|\\.)*"'
    values = set()
    for token in re.finditer(pattern, text):
        if token.group().startswith('R"'):
            values.add(token.group('raw'))
        elif token.group().startswith('"'):
            values.add(ast.literal_eval(token.group()))
    return values

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    args = parser.parse_args()
    assert VERSION == '0.8.53' and BASELINE_VERSION == '0.8.52'
    assert sha(args.baseline) == BASELINE_ARCHIVE_SHA256
    sketch = ROOT / SKETCH
    prefix = f'Tab5OS_Ecriture_v{BASELINE_VERSION}/Tab5OS_Arduino_v{BASELINE_VERSION}_Ecriture/'
    sources = sorted(p for p in sketch.iterdir() if p.suffix in {'.ino', '.cpp', '.h', '.inc'}
                     and p.name not in {'Tab5EnglishFallback.h', 'Tab5EnglishBitmap.h', 'Tab5SystemResourcesData.h'})
    hashes = {p.relative_to(ROOT).as_posix(): sha(p) for p in sources}
    delta_path = Path(__file__).with_name('ui-delta-0853.json')
    delta = json.loads(delta_path.read_text())
    packs, translations = {}, {}
    new_literals, technical, user_keys, unknown = {}, {}, set(), {}
    with zipfile.ZipFile(args.baseline) as archive:
        for path in sorted((sketch / 'sdcard/System/languages').glob('*.json')):
            relative = path.relative_to(sketch).as_posix()
            current = json.loads(path.read_text(), object_pairs_hook=no_duplicate_pairs)
            previous = json.loads(archive.read(prefix + relative), object_pairs_hook=no_duplicate_pairs)
            assert {k: v for k, v in current.items() if k != 'translations'} == {
                k: v for k, v in previous.items() if k != 'translations'}, (path.name, 'metadata')
            rows, old = current['translations'], previous['translations']
            assert all(rows.get(k) == v for k, v in old.items()), (path.name, 'existing entry changed')
            assert set(rows) - set(old) == set(delta[path.stem]) - set(old), (path.name, 'unreviewed addition')
            assert all(rows.get(k) == v for k, v in delta[path.stem].items()), path.name
            for key, value in delta[path.stem].items():
                assert value and signature(key) == signature(value), (path.name, key, 'format')
                assert key.count('\n') == value.count('\n'), (path.name, key, 'line breaks')
            packs[path.stem] = {'sha256': sha(path), 'entries': len(rows), 'baselineEntriesPreserved': len(old),
                                'reviewedAddedKeys': sorted(set(rows) - set(old))}
            translations[path.stem] = rows
            hashes[path.relative_to(ROOT).as_posix()] = sha(path)
        assert len(packs) == 14
        common_keys = set.intersection(*(set(v) for v in translations.values()))
        for path in sources:
            old_name = path.name.replace(VERSION, BASELINE_VERSION)
            try:
                previous = archive.read(prefix + old_name).decode('utf-8').replace('\r\n', '\n').replace('\r', '\n')
            except KeyError:
                previous = ''
            added = sorted(literals(path.read_text()) - literals(previous))
            if added:
                new_literals[path.name] = added
            for value in added:
                if not re.search(r'[A-Za-zÀ-ÿ]', value):
                    continue
                if value in common_keys:
                    user_keys.add(value)
                elif value in TECHNICAL:
                    technical[value] = TECHNICAL[value]
                elif re.fullmatch(r'[A-Za-z0-9_/.-]+\.(h|inc)', value):
                    technical[value] = 'Source include filename'
                else:
                    unknown.setdefault(path.name, []).append(value)
    assert not unknown, ('Review new literals before release', unknown)
    for path in sources:
        assert hashes[path.relative_to(ROOT).as_posix()] == sha(path), 'Source changed during audit'
    report = {'version': VERSION, 'passed': True,
              'scope': 'Preserve every .52 translation and metadata; verify exactly reviewed .53 additions and all newly introduced top-level production UI literals in 14 languages.',
              'baselineArchiveSha256': BASELINE_ARCHIVE_SHA256,
              'baselineMetadataPreserved': True, 'baselineTranslationsPreserved': True,
              'reviewedAdditionsPerPack': len(delta['fr-FR']),
              'sourceKeyCoverage': {'passed': True, 'keys': sorted(user_keys), 'technicalAdditions': technical},
              'packs': packs, 'sourceHashes': hashes,
              'testSourceHashes': {Path(__file__).relative_to(ROOT).as_posix(): sha(Path(__file__)),
                                  delta_path.relative_to(ROOT).as_posix(): sha(delta_path),
                                  'tools/audit_languages.py': sha(ROOT / 'tools/audit_languages.py'),
                                  'tools/release_config.py': sha(ROOT / 'tools/release_config.py')}}
    output = ROOT / f'tools/verification/evolutions-localization-{TAG}.json'
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'passed': True, 'packs': len(packs), 'reviewedAddedKeys': len(delta['fr-FR']),
                      'newSourceUiLiteralsCovered': len(user_keys), 'technicalAdditions': technical}))

if __name__ == '__main__':
    main()
