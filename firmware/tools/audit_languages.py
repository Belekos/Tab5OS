#!/usr/bin/env python3
"""Audit real pack coverage, printf arguments and installed font glyphs.

Does not fill untranslated text with English or claim that such entries are
translated. The report is suitable for comparing language pack updates.
"""
from pathlib import Path
import argparse
import json
import re
import struct
import hashlib
from datetime import datetime, timezone


def glyphs(path):
    data = path.read_bytes()
    tables = {}
    for i in range(struct.unpack_from('>H', data, 4)[0]):
        tag, _, offset, length = struct.unpack_from('>4sIII', data, 12 + i * 16)
        tables[tag] = (offset, length)
    base = tables[b'cmap'][0]
    result = set()
    for i in range(struct.unpack_from('>H', data, base + 2)[0]):
        platform, encoding, offset = struct.unpack_from('>HHI', data, base + 4 + i * 8)
        if platform not in (0, 3):
            continue
        start = base + offset
        kind = struct.unpack_from('>H', data, start)[0]
        if kind == 4:
            count = struct.unpack_from('>H', data, start + 6)[0] // 2
            ends = struct.unpack_from('>' + 'H' * count, data, start + 14)
            starts = struct.unpack_from('>' + 'H' * count, data, start + 16 + 2 * count)
            deltas = struct.unpack_from('>' + 'h' * count, data, start + 16 + 4 * count)
            ranges = start + 16 + 6 * count
            for j, (low, high, delta) in enumerate(zip(starts, ends, deltas)):
                ro = struct.unpack_from('>H', data, ranges + 2 * j)[0]
                for cp in range(low, high + 1):
                    glyph = struct.unpack_from('>H', data, ranges + 2 * j + ro + 2 * (cp - low))[0] if ro else cp
                    glyph = (glyph + delta) & 0xffff if glyph or not ro else 0
                    if glyph:
                        result.add(cp)
        elif kind == 12:
            for j in range(struct.unpack_from('>I', data, start + 12)[0]):
                low, high, first = struct.unpack_from('>III', data, start + 16 + 12 * j)
                result.update(range(low + (first == 0), high + 1))
    return result


def signature(text):
    # Compatible with Tab5TranslationFormat.h; literal percentages are allowed.
    result = []
    for m in re.finditer(r'%(?:%|[-+ #0\']*(\*|\d*)(?:\.(\*|\d*))?(hh|ll|[hljztL])?([diouxXfFeEgGaAcspn]))', text):
        if m.group(0) == '%%':
            continue
        if m.group(1) == '*':
            result.append('*')
        if m.group(2) == '*':
            result.append('*')
        result.append('%' + (m.group(3) or '') + m.group(4))
    return result


def no_duplicate_pairs(pairs):
    d = {}
    for k, v in pairs:
        if k in d:
            raise ValueError('Duplicate JSON key: ' + k)
        d[k] = v
    return d


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--sketch', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    release = Path(__file__).resolve().parents[1]
    sketch = args.sketch or next(release.glob('Tab5OS_Arduino_*_Ecriture'))
    folder = sketch / 'sdcard/System/languages'
    load = lambda p: json.loads(p.read_text(encoding='utf-8-sig'), object_pairs_hook=no_duplicate_pairs)
    reference = load(folder / 'en-US.json')['translations']
    font = glyphs(sketch / 'sdcard/System/font.ttf')
    keyboards = {x['id'] for x in load(sketch/'sdcard/System/keyboards.json')['keyboards']}
    version = re.search(r'_v(\d+\.\d+\.\d+)_', sketch.name).group(1)
    report = {'release':version,'checkedAt':datetime.now(timezone.utc).isoformat(),'referenceEntries': len(reference), 'fontGlyphs': len(font), 'packs': {}}
    problems = []
    for path in sorted(folder.glob('*.json')):
        pack = load(path)
        repertoire=pack['id'][:2]
        cjk_path=sketch/'sdcard/System/fonts'/('cjk-'+repertoire+'.ttf')
        supported=font|glyphs(cjk_path) if repertoire in ('zh','ja','ko') and cjk_path.exists() else font
        rows = pack['translations']
        invalid = [k for k, v in rows.items() if not isinstance(v, str) or not v or signature(k) != signature(v)]
        missing = sorted(reference.keys() - rows.keys())
        # Option lists and shortcut rows must retain their row count. Long help
        # paragraphs may be regrouped by translators without changing the UI.
        newlines = [k for k, v in rows.items() if isinstance(v, str) and '\n\n' not in k and k.count('\n') != v.count('\n')]
        paragraph_breaks = [k for k, v in rows.items() if isinstance(v, str) and '\n\n' in k and k.count('\n') != v.count('\n')]
        unchanged = [k for k, v in rows.items() if pack['id'] not in ('fr-FR', 'en-US') and len(v) > 90 and (v == k or v == reference.get(k))]
        unsupported = sorted({ord(c) for v in rows.values() for c in v if ord(c) >= 32 and ord(c) not in supported and c != '\x7f'})
        coverage = {
            'entries': len(rows), 'missingKeys': len(missing),
            'coveragePercent': round(100 * len(reference.keys() & rows.keys()) / len(reference), 2),
            'formatErrors': invalid,
            'lineBreakErrors': newlines,
            'paragraphBreakDifferences': paragraph_breaks,
            'untranslatedLongText': unchanged,
            'unsupportedGlyphs': ['U+%04X %s' % (cp, chr(cp)) for cp in unsupported],
            'bytes': path.stat().st_size,
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'keyboard': pack.get('keyboard'),
            'keyboardAvailable': pack.get('keyboard') in keyboards,
        }
        report['packs'][pack['id']] = coverage
        if invalid or missing or unsupported or newlines or unchanged or path.stat().st_size > 512 * 1024 or len(rows) > 4096 or pack.get('keyboard') not in keyboards:
            problems.append(path.name)
    report['passed']=not problems
    target = args.output or release / ('tools/verification/languages-' + version.replace('.', '') + '.json')
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2))
    if problems:
        raise SystemExit('Invalid packs: ' + ', '.join(problems))


if __name__ == '__main__':
    main()
