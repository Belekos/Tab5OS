#!/usr/bin/env python3
"""Audit current evolution labels against delivered 0.8.41 and all 14 packs.

The old localization report is not relabelled. This audit compares current
production strings to the authenticated baseline, verifies every added
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
SKETCH = ROOT / 'Tab5OS_Arduino_v0.8.42_Ecriture'
BASELINE_SHA256 = '01389322d10ae80b668ce369b2586e41831af03e3e407ecd3d60da62dd2667ff'
BASELINE_PREFIX = 'Tab5OS_Ecriture_v0.8.41/Tab5OS_Arduino_v0.8.41_Ecriture/'
sys.path.insert(0, str(ROOT / 'tools'))
from audit_languages import no_duplicate_pairs, signature

TECHNICAL = {
    '0.8.42': 'Firmware version identifier',
    '/USB': 'Logical USB root path',
    '/USB/': 'Logical USB path prefix',
    'Git': 'Product/protocol name, identical in all languages',
    'SD': 'Storage technology name, identical in all languages',
    '/MicroPython/libraries.py': 'English filesystem path for the shipped library utility',
    'New folder': 'English default folder name required independently of UI language',
    ' - copy': 'English duplicate-name suffix required independently of UI language',
}


C_KEYWORDS = set("_Alignas _Alignof _Atomic _Bool _Complex _Generic _Imaginary _Noreturn _Static_assert _Thread_local alignas alignof auto bool char double extern float inline int long register restrict short signed sizeof union unsigned volatile false null true".split())
IDENTIFIERS = {
    "Meshtastic": "Product/protocol name, identical in all languages",
    "fr-FR": "Language identifier for the source-language fallback policy",
    "USB IPP": "Standard transport/protocol names",
    " · USB": "Standard USB transport suffix",
    "ipp://localhost/ipp/print": "IPP printer URI",
    "usb://printer": "Internal USB printer endpoint URI",
    "tab5_usb_printer": "FreeRTOS task name",
    "POST /ipp/print HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/ipp\r\nContent-Length: %u\r\nConnection: keep-alive\r\n\r\n": "HTTP/IPP wire-format request template",

    "Meshtastic · Bluetooth": "Two standard product/protocol names",
    **{k: "Structural presentation template; words are translated separately" for k in ("\n%s !%08lx", "%s", "%s\n%02X:%02X:%02X:%02X:%02X:%02X · %d dBm", "%s !%08lx · %s %u\n%s\n\n", "%s %06lu", "%s (%d)")},
    **{k: "HTTP protocol token" for k in ("HTTP/1.0 ", "HTTP/1.1 ", "chunked", "content-length", "transfer-encoding")},
    **{k: "Preferences key/namespace" for k in ("document_font", "interface_font", "tab5_meshtastic")},
    **{k: "Machine-readable serial diagnostic record" for k in ("document_font=%s\n", "interface_font=%s\n")},
    "Font": "Memory-reservation operation name used only in serial diagnostics",
    "Auto": "Internal codec mode; the document UI stores and shows a concrete encoding",
    "C": "C language identifier",
    "CFF ": "TrueType table signature", "CFF2": "TrueType table signature",
    **{k: "TrueType table signature" for k in ("cmap", "fvar", "glyf", "head", "hhea", "hmtx", "loca", "maxp")},
    "HTML": "Standard document format name",
    "FAT": "Filesystem format name", "FAT32": "Filesystem format name",
    "UTF-8": "Standard encoding name", "UTF-8 BOM": "Standard encoding name",
    "UTF-16 LE": "Standard encoding name", "UTF-16 BE": "Standard encoding name",
    "Windows-1252": "Standard encoding name",
    "meshtastic": "Internal task/resource name", "meshtastic BLE": "Serial diagnostic operation name",
    "tab5_mesh_ble": "FreeRTOS task name", "tab5_usb_format": "FreeRTOS task name", "usb-format": "Internal storage operation name",
    "screen_upside_down": "Preferences key", "screen_upside_down=%u\n": "Machine-readable serial diagnostic record",
    "data.json": "English default filename", "document.md": "English default filename", "program.c": "English default filename",
    "draft-%04u%s": "English default filename template", "Draft-%04u%s": "English default filename template",
}

SOURCE_TECHNICAL = {
    "Tab5MeshAllocatorEspIdf554.inc": "Verbatim ESP-IDF allocator; allocation/debug log strings are never UI messages",
    "WriterSyntaxWords.inc": "Lexer vocabularies: programming-language tokens, never user-interface messages",
}

IDENTIFIERS.update({
    "\n/System/maps/gazetteer/cities.t5geo": "Offline gazetteer filesystem path shown after a localized error",
    "ISR counters must be lock-free": "Compile-time static_assert diagnostic, never a user-interface message",
    **{k: "Programming language/format proper name" for k in ("C++ / Arduino","CMake","CSS / SCSS","Go","HTML / XML","INI","JSON","Java","JavaScript","Kotlin","Lua","Makefile","PHP","PowerShell","Ruby","Rust","SQL","Shell","Swift","TOML","TypeScript","YAML")},
    **{k: "Preferences key/namespace" for k in ("settings","tab5-mesh")},
    "mesh_store": "Internal worker/resource identifier",
    "T5GEO01\x00": "Binary gazetteer format signature",
    "define": "C preprocessor token",
    "GeoNames · CC BY 4.0": "Data source and license proper names",
    "!%08lx": "Meshtastic node identifier presentation",
    "\n!%08lx": "Meshtastic node identifier presentation",
    "\n%Y-%m-%d %H:%M UTC": "ISO numeric UTC timestamp",
    **{k: "C++ raw-string literal prefix" for k in ('LR"','UR"','u8R"','uR"')},
    "T5WS0002": "Versioned binary writer workspace signature",
    "%u %%": "Numeric percentage format",
    "%Y-%m-%d %H:%M:%S": "ISO numeric timestamp for cross-language history",
    **{k: "Structural presentation template; words translated separately" for k in ("%08lx","%s · %s%s%s","Meshtastic · !%08lx","%lu","%s %s !%08lx · %s","%s%s\n%02X:%02X:%02X:%02X:%02X:%02X%s","%u–%u / %u","%u")},
})

IDENTIFIERS.update({
    **{k: "Structural numeric layout; phase names are translated separately" for k in ("\n  %s : %lu ms", "%s%lu ms | %s | %lu us")},
    **{k: "Memory region label in explicit serial diagnostics" for k in ("HP SRAM heap", "INTERNAL aggregate", "LP SRAM heap", "PSRAM heap", "SPM/TCM heap")},
    **{k: "Machine-readable or human-readable serial diagnostic, never a UI message" for k in (
        "%s %llu %llu %llu %llu\n", "[UI previous reset]\n%s\n", "[UI recent]\n%s\n",
        "[UI] %s: calls=%lu total=%llu us max=%lu us >=20ms=%lu\n", "[UI] profiling %s\n",
        "[UI] wall time (not CPU), enabled=%u, max gap=%lu us\n", "memory bytes: total free minimum_free largest_block\n",
        "minimum_free sums each registered region's low-water mark; INTERNAL includes HP+SPM/TCM+LP, not additional memory\n",
        "physical HP=%llu SPM/TCM=%llu LP=%llu L2_cache=%llu HP_code_data_reserved=%llu\n")},
    "Keep retained UI diagnostics bounded": "Compile-time size assertion, not a UI message",
    "chargement ELF": "Memory reservation operation label for serial diagnostics",
    "resource repair": "Memory reservation operation label for serial diagnostics",
    "finalisation enregistrement": "Recording error operation name for serial diagnostics",
    "on": "Serial diagnostic boolean token", "off": "Serial diagnostic boolean token",
    "en": "Language code for offline lookup", "fr": "Language code for offline lookup",
    "path=%s\nchapter=%u\n%soffset=%lu\nscroll=%ld\n": "Persistent reading-state record format",
})

def technical_reason(value, source=None):
    if source in SOURCE_TECHNICAL: return SOURCE_TECHNICAL[source]
    if value in IDENTIFIERS: return IDENTIFIERS[value]
    if value in C_KEYWORDS: return "C/JSON syntax token used by the editor/parser"
    if value.startswith('/') and not re.search(r'\s', value): return "Filesystem path"
    if re.fullmatch(r'\.[A-Za-z0-9]+', value): return "Filename extension"
    return None


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def literals(text):
    values = set()
    pattern = r'//[^\n]*|/\*[\s\S]*?\*/|R"(?P<delimiter>[^\s()\\]{0,16})\((?P<raw>[\s\S]*?)\)(?P=delimiter)"|\'(?:[^\'\\\n]|\\.)*\'|"(?:[^"\\]|\\.)*"'
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
    assert sha(args.baseline) == BASELINE_SHA256, 'Not the delivered 0.8.41 ZIP'
    sources = sorted([*SKETCH.glob('*.ino'), *SKETCH.glob('*.cpp'), *SKETCH.glob('*.h'), *SKETCH.glob('*.inc'),
                      *[p for p in (SKETCH/'tools/native-apps/image-editor').iterdir()
                        if p.suffix.lower() in {'.c','.h','.inc'} and not p.name.startswith('test_')]])
    sources = [p for p in sources if p.name not in {'Tab5EnglishFallback.h','Tab5EnglishBitmap.h','Tab5SystemResourcesData.h'}]
    hashes = {str(path.relative_to(ROOT)): sha(path) for path in sources}
    keys = set()
    source_keys = {}
    excluded = {}
    packs = {}
    with zipfile.ZipFile(args.baseline) as archive:
        assert archive.testzip() is None, 'Baseline ZIP CRC mismatch'
        for path in sources:
            relative = path.relative_to(SKETCH).as_posix()
            old_name = relative.replace('0.8.42', '0.8.41')
            try:
                previous = literals(archive.read(BASELINE_PREFIX + old_name).decode('utf-8').replace('\r\n','\n').replace('\r','\n'))
            except KeyError:
                previous = set()
            added = literals(path.read_text(encoding='utf-8')) - previous
            source_keys[relative] = []
            for value in sorted(added):
                if not value or not re.search('[A-Za-zÀ-ÿ]', value):
                    continue
                reason = TECHNICAL.get(value) or technical_reason(value, relative)
                if value.endswith(('.h', '.inc')):
                    reason = 'Source include filename'
                if reason:
                    excluded[value] = reason
                    continue
                keys.add(value)
                source_keys[relative].append(value)
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
    for file in ('tools/localization/evolutions-delta-0842.json','tools/localization/apply_evolutions_0842.py'):
        hashes[file] = sha(ROOT / file)
    report = {
        'version': '0.8.42', 'passed': True, 'method': __doc__,
        'baselineZipSha256': BASELINE_SHA256,
        'baselineTranslationsAndMetadataPreserved': True,
        'packs': packs, 'sourceHashes': hashes,
        'sourceKeyCoverage': {
            'passed': True, 'translatedKeysInChangedSources': len(keys),
            'missingKeys': [], 'keysBySource': source_keys, 'technicalExclusions': excluded,
            'method': 'Current production source literal additions against authenticated delivered 0.8.41; all 14 packs checked without rewriting existing translations.',
        },
        'jsonCompaction': 'Whitespace removed only; baseline metadata and every previous translation value are compared semantically.',
        'limitations': ['Static source and pack verification; no physical device or live language-switch test.'],
    }
    target = ROOT / 'tools/verification/evolutions-localization-0842.json'
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'passed': True, 'addedSourceKeys': len(keys), 'packs': len(packs),
                      'unchangedPacks': sum(row['byteIdenticalToBaseline'] for row in packs.values()),
                      'report': str(target)}))


if __name__ == '__main__':
    main()
