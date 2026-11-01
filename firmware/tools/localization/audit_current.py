#!/usr/bin/env python3
"""Audit current evolution labels against the delivered baseline and all 14 packs.

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
sys.path.insert(0, str(ROOT / 'tools'))
from release_config import VERSION, TAG, SKETCH as SKETCH_NAME, BASELINE_VERSION, BASELINE_ARCHIVE_SHA256
SKETCH = ROOT / SKETCH_NAME
BASELINE_SHA256 = BASELINE_ARCHIVE_SHA256
BASELINE_PREFIX = f'Tab5OS_Ecriture_v{BASELINE_VERSION}/Tab5OS_Arduino_v{BASELINE_VERSION}_Ecriture/'
from audit_languages import no_duplicate_pairs, signature
from apply_delta_0851 import obsolete_key, checked_live_sources, deltas

TECHNICAL = {
    '@alarm': 'Internal vector icon identifier, never displayed as a text label',
    '%u /': 'Language-independent numeric measured/required fragment; the surrounding admission template is localized',
    'LVGL backing is already included in PSRAM usage; unused pool bytes are reusable, not additional physical RAM\n': 'Explicit serial memory command explanation; never a user-interface label',
    'LVGL private: backing_bytes=%llu used_bytes=%llu live_blocks=%llu pools=%llu peak_used=%llu\n': 'Explicit serial memory command numeric record; never a user-interface label',
    'Tab5OS requires its supplied LVGL configuration (custom pool adapter and native TLSF)': 'Compile-time #error, not firmware user-interface text',
    '[MEM] %s | interne=%u Kio min=%u Kio | ': 'Serial-only counter diagnostic; no user-interface text',
    'PSRAM=%u Kio min=%u Kio | DMA interne=%u Kio | caches liberes=%u Kio refus=%u\n': 'Serial-only counter diagnostic; no user-interface text',
    'Two compact physical spans per row': 'Compile-time static_assert for framebuffer mirror metadata size',
    '[MAP][SEARCH] fallback HTTP=%d TLS=%d results=%u\n': 'Serial-only online-to-local search fallback diagnostic',
    '[DISPLAY config] available=0\n': 'Serial-only existing display configuration availability diagnostic',
    '[DISPLAY config] available=1 requestedPixelClockMHz=%u width=%u height=%u rotation=%u offsetRotation=%u hBack=%u hPulse=%u hFront=%u vBack=%u vPulse=%u vFront=%u\n': 'Serial-only existing display driver configuration snapshot; no pixel clock is changed',
    '[SD] echecs=%u, recuperations=%u, utilisateurs=%u, fichiers=%u, exclusif=%u, disponible=%u, errno=%d, attente=%d, essais=%u/%u, limite=%d\n': 'Serial-only storage recovery/lease diagnostic',
    '[radio] ouverture du flux distant\n': 'Serial-only redacted radio diagnostic',
    '[DISPLAY] ms=%lu page=%u active=%u scans=%lu submissions=%lu flips=%lu pending=%u timeouts=%lu copyErrors=%lu publishErrors=%lu lastError=%ld\n': "Serial-only diagnostic text, never a user-interface message",
    '[DSI] observed=%u irqFlags=0x%lx interrupts=%lu underruns=%lu lastUnderrunUs=%llu status=0x%08lx\n': "Serial-only diagnostic text, never a user-interface message",
    '[DSI underrun] delta=%lu total=%lu page=%u atUs=%llu\n': 'Deferred serial-only hardware underrun diagnostic',
    '[LVGL source] %s=%lu\n': "Serial-only diagnostic text, never a user-interface message",
    '[LVGL] observed=%u frames=%lu areas=%lu pixels=%llu lastFlushMs=%lu largeFrames=%lu lastLargeFrameMs=%lu lastLargeInvalidationMs=%lu source=%s pixels=%lu\n': "Serial-only diagnostic text, never a user-interface message",
    'flux distant': "Serial-only diagnostic text, never a user-interface message",
    "connexion HTTP/TLS video": "Retained network-stage diagnostic, never a user-interface message",
    "lecture flux video": "Retained network-stage diagnostic, never a user-interface message",
    "fichier temporaire": "Storage error operation label used only in diagnostics",
    '[APPS] Catalogue worker unavailable; incremental scan\n': 'Serial-only worker fallback diagnostic',
    'catalogue': 'Resource admission operation identifier',
    'tab5_catalogue': 'FreeRTOS task name',
    'home preferences must preserve their on-disk size': 'Compile-time static_assert diagnostic',
    '[UI] profiling allocation failed': 'Serial-only diagnostic for an optional profiler allocation failure',
    VERSION: 'Firmware version identifier',
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
    "%s %s": "Structural caption/value template; caption is translated before formatting",
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
    'Two compact physical spans per row': 'Compile-time static_assert for framebuffer mirror metadata size',
    '[MAP][SEARCH] fallback HTTP=%d TLS=%d results=%u\n': 'Serial-only online-to-local search fallback diagnostic',
    '[DISPLAY config] available=0\n': 'Serial-only existing display configuration availability diagnostic',
    '[DISPLAY config] available=1 requestedPixelClockMHz=%u width=%u height=%u rotation=%u offsetRotation=%u hBack=%u hPulse=%u hFront=%u vBack=%u vPulse=%u vFront=%u\n': 'Serial-only existing display driver configuration snapshot; no pixel clock is changed',
    '[SD] echecs=%u, recuperations=%u, utilisateurs=%u, fichiers=%u, exclusif=%u, disponible=%u, errno=%d, attente=%d, essais=%u/%u, limite=%d\n': 'Serial-only storage recovery/lease diagnostic',
    '[radio] ouverture du flux distant\n': 'Serial-only redacted radio diagnostic',
    '[APPS] Catalogue worker unavailable; incremental scan\n': 'Serial-only worker fallback diagnostic',
    'catalogue': 'Resource admission operation identifier',
    'tab5_catalogue': 'FreeRTOS task name',
    'home preferences must preserve their on-disk size': 'Compile-time static_assert diagnostic',
    '[UI] profiling allocation failed': 'Serial-only diagnostic for an optional profiler allocation failure',
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

IDENTIFIERS.update({
    "Tab5OS requires the isolated M5Unified audio startup patch; use the supplied build scripts": "Compile-time #error diagnostic requiring the checked dependency, never a UI message",
    "media_prefs": "FreeRTOS task name for the settings writer, never a UI message",
    "hidden syntax indexes": "Memory-reclaimer diagnostic label, never a UI message",
    "[DISPLAY transfer] nativeRects=%lu nativePixels=%llu maxBlitUs=%lu maxRefreshGapUs=%lu\n": "Explicit serial diagnostic format, never a UI message",
    "[MEDIA PREFS] started=%lu completed=%lu failed=%lu deferred=%lu pending=%u lastUs=%lu maxUs=%lu intervalUs=%llu..%llu dsiObserved=%u underrunsDuringWrites=%lu\n": "Explicit serial diagnostic format, never a UI message",
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
    assert sha(args.baseline) == BASELINE_SHA256, f'Not the delivered {BASELINE_VERSION} ZIP'
    sources = sorted([*SKETCH.glob('*.ino'), *SKETCH.glob('*.cpp'), *SKETCH.glob('*.h'), *SKETCH.glob('*.inc'),
                      *[p for p in (SKETCH/'tools/native-apps/image-editor').iterdir()
                        if p.suffix.lower() in {'.c','.h','.inc'} and not p.name.startswith('test_')]])
    sources = [p for p in sources if p.name not in {'Tab5EnglishFallback.h','Tab5EnglishBitmap.h','Tab5SystemResourcesData.h'}]
    unused_scan = checked_live_sources(SKETCH)
    hashes = {str(path.relative_to(ROOT)): sha(path) for path in sources}
    keys = set()
    source_keys = {}
    excluded = {}
    packs = {}
    with zipfile.ZipFile(args.baseline) as archive:
        assert archive.testzip() is None, 'Baseline ZIP CRC mismatch'
        for path in sources:
            relative = path.relative_to(SKETCH).as_posix()
            old_name = relative.replace(VERSION, BASELINE_VERSION)
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
            old_help = obsolete_key(original['translations'])
            assert old_help not in rows
            reviewed = deltas()[path.stem]
            assert rows['Relief'] == reviewed['Relief'], (path.name, 'relief terminology')
            assert all(rows.get(k) == v for k, v in original['translations'].items()
                       if k not in (old_help, 'Relief')), (path.name, 'unreviewed existing translation changed')
            assert set(original['translations']) - set(rows) == {old_help}, (path.name, 'unexpected removed key')
            assert all(rows[k] == v for k, v in reviewed.items()), (path.name, 'reviewed delta differs')
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
    hashes['tools/release_config.py'] = sha(ROOT / 'tools/release_config.py')
    for delta in [HERE/'apply_delta_0851.py', *sorted(HERE.glob('relief-*-delta-0851.json'))]:
        hashes[str(delta.relative_to(ROOT))] = sha(delta)
    report = {
        'version': VERSION, 'passed': True, 'method': __doc__,
        'baselineZipSha256': BASELINE_SHA256,
        'baselineMetadataPreserved': True,
        'baselineTranslationsPreservedExceptReviewedChanges': True,
        'explicitObsoleteKeyMigration': {'countPerPack': 1, 'removedKeySha256': hashlib.sha256(old_help.encode()).hexdigest(), 'unusedReferenceScanFiles': len(unused_scan), 'reason': 'Superseded Explorer help absent from all live C/C++/Python/ELF sources; current Explorer help is unchanged'},
        'explicitTerminologyChange': {'key': 'Relief', 'reason': 'May be raised or recessed; no longer translated as raised surface only'},
        'packs': packs, 'sourceHashes': hashes,
        'sourceKeyCoverage': {
            'passed': True, 'translatedKeysInChangedSources': len(keys),
            'missingKeys': [], 'keysBySource': source_keys, 'technicalExclusions': excluded,
            'method': f'Current production source literal additions against authenticated delivered {BASELINE_VERSION}; all 14 packs checked with reviewed relief terminology and one unreachable help key removed.',
        },
        'jsonCompaction': 'Existing compact JSON preserved; no parser limits raised. All baseline entries and metadata compared, with only the two reviewed exceptions above.',
        'limitations': ['Static source and pack verification; no physical device or live language-switch test.'],
    }
    target = ROOT / f'tools/verification/evolutions-localization-{TAG}.json'
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'passed': True, 'addedSourceKeys': len(keys), 'packs': len(packs),
                      'unchangedPacks': sum(row['byteIdenticalToBaseline'] for row in packs.values()),
                      'report': str(target)}))


if __name__ == '__main__':
    main()
