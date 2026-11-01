#!/usr/bin/env python3
"""Verify unchanged language packs and new source literals against delivered .51."""
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
    assert VERSION == '0.8.52' and BASELINE_VERSION == '0.8.51'
    assert sha(args.baseline) == BASELINE_ARCHIVE_SHA256
    sketch = ROOT / SKETCH
    prefix = f'Tab5OS_Ecriture_v{BASELINE_VERSION}/Tab5OS_Arduino_v{BASELINE_VERSION}_Ecriture/'
    sources = sorted(p for p in sketch.iterdir() if p.suffix in {'.ino', '.cpp', '.h', '.inc'}
                     and p.name not in {'Tab5EnglishFallback.h', 'Tab5EnglishBitmap.h', 'Tab5SystemResourcesData.h'})
    hashes = {p.relative_to(ROOT).as_posix(): sha(p) for p in sources}
    additions, packs = {}, {}
    with zipfile.ZipFile(args.baseline) as archive:
        for path in sources:
            previous = archive.read(prefix + path.name.replace(VERSION, BASELINE_VERSION)).decode('utf-8').replace('\r\n', '\n').replace('\r', '\n')
            added = sorted(literals(path.read_text()) - literals(previous))
            if added:
                additions[path.name] = added
            # This release introduces no labels or translatable messages.
            assert all(value == VERSION or re.fullmatch(r'[A-Za-z0-9_/.-]+\.(h|inc)', value)
                       for value in added), (path.name, added)
        for path in sorted((sketch / 'sdcard/System/languages').glob('*.json')):
            relative = path.relative_to(sketch).as_posix()
            assert path.read_bytes() == archive.read(prefix + relative), relative
            pack = json.loads(path.read_text())
            packs[path.stem] = {'sha256': sha(path), 'entries': len(pack['translations']),
                                'byteIdenticalToBaseline': True}
            hashes[path.relative_to(ROOT).as_posix()] = sha(path)
    assert len(packs) == 14
    for path in sources:
        assert hashes[path.relative_to(ROOT).as_posix()] == sha(path)
    report = {'version': VERSION, 'passed': True,
              'scope': 'Byte-identical preservation of all 14 packs and audit of new literals in current top-level production sources; no new UI strings.',
              'baselineArchiveSha256': BASELINE_ARCHIVE_SHA256,
              'baselineMetadataPreserved': True, 'baselineTranslationsByteIdentical': True,
              'sourceKeyCoverage': {'passed': True, 'newUserFacingKeys': 0, 'technicalAdditions': additions},
              'packs': packs, 'sourceHashes': hashes,
              'testSourceHashes': {Path(__file__).relative_to(ROOT).as_posix(): sha(Path(__file__)),
                                   'tools/release_config.py': sha(ROOT / 'tools/release_config.py')}}
    output = ROOT / f'tools/verification/evolutions-localization-{TAG}.json'
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'passed': True, 'packs': len(packs), 'newUiKeys': 0, 'technicalAdditions': additions}))

if __name__ == '__main__':
    main()
