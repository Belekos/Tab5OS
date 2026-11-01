#!/usr/bin/env python3
"""Audit Dessin literals and new Explorer image literals in all 14 packs.

The entire editor, codec and core are scanned. For the large existing Explorer
and image decoder, only added literals are considered, against the authenticated
delivered 0.8.34 archive. Technical strings require an explicit exclusion.
"""
from pathlib import Path
import argparse
import ast
import hashlib
import json
import re
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SKETCH = ROOT / 'Tab5OS_Arduino_v0.8.35_Ecriture'
BASELINE_SHA256 = '04ddb6b02b3517d9a146d5e2d83a1a5b046db765de07f7117324b7a4e791eb17'
BASELINE_SKETCH = 'Tab5OS_Ecriture_v0.8.34/Tab5OS_Arduino_v0.8.34_Ecriture/'
FULL_SOURCES = ['Tab5ImageEditor.cpp', 'Tab5ImageCodec.cpp', 'tools/native-apps/image-editor/image_core.c']
DELTA_SOURCES = ['Tab5OS_Arduino_v0.8.35_Ecriture.ino', 'Tab5ExplorerMenus.inc', 'Tab5ExplorerImages.inc', 'Tab5ImageDecode.cpp', 'Tab5ImageFormats.h', 'Tab5WebPPort.cpp']
EXCLUDED = {
    '.bmp', '.t5i', '.jpg', '.jpeg', '.png', '.gif', '.webp', 'T5IMG001', 'RIFF', 'WEBP', '0.8.35',
    ' · (%d, %d) → (%d, %d) · %d × %d · %s %.1f · %s %.1f°',
    '%d × %d · %d%% · %s %d/%d · %s %u/%u%s%s%s%s',
    '%d%%', '%d', '#%06X', '%s%d · %s', '%s %u/%u%s', ' · (%d, %d)',
    '(%d, %d) · %d × %d · %d%% · %s %u/%u%s',  # numbers plus separately translated history label
    '\x89PNG\r\n\x1a\n',  # binary PNG signature, never displayed
}


def literals(source):
    values = set()
    for token in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:[^"\\]|\\.)*"', source):
        if token.group().startswith('"'):
            values.add(ast.literal_eval(token.group()))
    return values


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', type=Path, required=True)
    args = parser.parse_args()
    assert hashlib.sha256(args.baseline.read_bytes()).hexdigest() == BASELINE_SHA256
    report_path = ROOT / 'tools/verification/drawing-localization-0835.json'
    report = json.loads(report_path.read_text(encoding='utf-8'))
    packs = {p.stem: json.loads(p.read_text(encoding='utf-8'))['translations'] for p in (SKETCH / 'sdcard/System/languages').glob('*.json')}
    keys, hashes, unknown = set(), {}, []
    by_source = {}
    with zipfile.ZipFile(args.baseline) as archive:
        for name in FULL_SOURCES + DELTA_SOURCES:
            path = SKETCH / name
            if not path.exists():
                assert name in DELTA_SOURCES, name
                continue
            hashes[str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
            values = literals(path.read_text(encoding='utf-8'))
            if name in DELTA_SOURCES:
                old_name = name.replace('v0.8.35', 'v0.8.34')
                try:
                    values -= literals(archive.read(BASELINE_SKETCH + old_name).decode('utf-8'))
                except KeyError:
                    pass
            source_keys = []
            for value in sorted(values):
                if not value or not re.search('[A-Za-zÀ-ÿ]', value):
                    continue
                if value in EXCLUDED or value.endswith(('.h', '.inc')) or value.startswith('/Images'):
                    continue
                keys.add(value)
                source_keys.append(value)
                if value not in packs['en-US']:
                    unknown.append((name, value))
            by_source[name] = source_keys
    assert not unknown, unknown
    assert len(packs) == 14
    for locale, pack in packs.items():
        assert not keys - pack.keys(), (locale, sorted(keys - pack.keys()))
    for name in ['drawing-delta-0835.json', 'build_drawing_delta_0835.py', 'apply_drawing_0835.py', 'audit_drawing_keys_0835.py']:
        path = HERE / name
        hashes[str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
    report['sourceHashes'] = hashes
    report['sourceKeyCoverage'] = {
        'passed': True, 'translatedKeysInImageSources': len(keys),
        'missingKeys': [], 'keysBySource': by_source,
        'method': 'All editor, codec and core literals; new Explorer and decoder literals against verified 0.8.34 archive; explicit exclusions for technical strings.',
    }
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('PASS', len(keys), 'source keys covered by', len(packs), 'packs')


if __name__ == '__main__':
    main()
