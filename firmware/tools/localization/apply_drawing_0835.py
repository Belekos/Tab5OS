#!/usr/bin/env python3
"""Apply the reviewed 0.8.35 translations without changing the delivered baseline.

The source archive is the final 0.8.34 release. Existing translations and pack
metadata are immutable; additions retain printf arguments and line structure.
"""
from pathlib import Path
import argparse
import hashlib
import json
import sys
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SKETCH = ROOT / 'Tab5OS_Arduino_v0.8.35_Ecriture'
BASELINE_SHA256 = '04ddb6b02b3517d9a146d5e2d83a1a5b046db765de07f7117324b7a4e791eb17'
BASELINE_SKETCH = 'Tab5OS_Ecriture_v0.8.34/Tab5OS_Arduino_v0.8.34_Ecriture/'
sys.path.insert(0, str(ROOT / 'tools'))
from audit_languages import signature, no_duplicate_pairs


def read(path):
    return json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=no_duplicate_pairs)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', type=Path, required=True)
    args = parser.parse_args()
    baseline_sha = hashlib.sha256(args.baseline.read_bytes()).hexdigest()
    assert baseline_sha == BASELINE_SHA256, 'Not the delivered 0.8.34 archive'
    delta = read(HERE / 'drawing-delta-0835.json')
    keys = set(delta['fr-FR'])
    assert all(set(rows) == keys for rows in delta.values()), 'Different key sets'
    assert len(delta) == 14, 'All 14 languages are required'
    report = {
        'version': '0.8.35', 'passed': True, 'baselineZipSha256': baseline_sha,
        'reviewedKeys': len(keys), 'baselineTranslationsAndMetadataPreserved': True,
        'packs': {},
    }
    with zipfile.ZipFile(args.baseline) as archive:
        for locale, rows in sorted(delta.items()):
            rel = 'sdcard/System/languages/' + locale + '.json'
            orig = json.loads(archive.read(BASELINE_SKETCH + rel), object_pairs_hook=no_duplicate_pairs)
            path = SKETCH / rel
            pack = read(path)
            assert {k: v for k, v in pack.items() if k != 'translations'} == {k: v for k, v in orig.items() if k != 'translations'}, (locale, 'metadata changed')
            assert all(pack['translations'].get(k) == v for k, v in orig['translations'].items()), (locale, 'existing translation changed')
            for key, value in rows.items():
                assert isinstance(value, str) and value, (locale, key, 'empty')
                assert signature(key) == signature(value), (locale, key, 'format')
                assert key.count('\n') == value.count('\n'), (locale, key, 'newlines')
                assert key not in orig['translations'] or orig['translations'][key] == value, (locale, key, 'old key conflict')
                pack['translations'][key] = value
            data = (json.dumps(pack, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
            assert len(data) <= 512 * 1024, (locale, len(data), 'size limit')
            assert len(pack['translations']) <= 4096, (locale, 'entry limit')
            names = {k: len(v.encode('utf-8')) for k, v in pack['translations'].items() if k in ('Image BMP', 'Image JPEG', 'Image PNG', 'Image WebP')}
            assert max(names.values(), default=0) <= 23, (locale, 'layer name length')
            path.write_bytes(data)
            report['packs'][locale] = {
                'newKeys': len(set(rows) - set(orig['translations'])),
                'entries': len(pack['translations']), 'bytes': len(data),
                'sha256': hashlib.sha256(data).hexdigest(), 'layerNameBytes': names,
            }
    report['newKeys'] = report['packs']['en-US']['newKeys']
    (ROOT / 'tools/verification/drawing-localization-0835.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('PASS', len(keys), 'reviewed keys;', len(delta), 'packs; Russian', report['packs']['ru-RU']['bytes'], 'bytes')


if __name__ == '__main__':
    main()
