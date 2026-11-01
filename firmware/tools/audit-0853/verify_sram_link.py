"""Check the actual linked placement of the .53 SRAM changes against authenticated .52."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from release_config import VERSION, TAG, SKETCH, BASELINE_VERSION, BASELINE_ARCHIVE_SHA256

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def symbols(objdump, elf):
    text = subprocess.check_output([str(objdump), '-t', '-C', str(elf)], text=True)
    rows = {}
    for line in text.splitlines():
        match = re.match(r'^\s*([0-9a-f]+)\s+\w+\s+O\s+(\S+)\s+([0-9a-f]+)\s+(.+)$', line)
        if match:
            rows[match[4]] = {'address': '0x' + match[1], 'section': match[2], 'bytes': int(match[3], 16)}
    return rows

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment', required=True, type=Path)
    parser.add_argument('--baseline-zip', required=True, type=Path)
    parser.add_argument('--baseline-release', required=True, type=Path)
    args = parser.parse_args()
    env = args.environment.resolve()
    assert sha(args.baseline_zip) == BASELINE_ARCHIVE_SHA256
    with zipfile.ZipFile(args.baseline_zip) as archive:
        old_report = json.loads(archive.read(f'Tab5OS_Ecriture_v{BASELINE_VERSION}/tools/verification/firmware-verification-{BASELINE_VERSION.replace(".", "")}.json'))
    old_artifact = next(row for row in old_report['artifacts'] if row['suffix'] == '.elf')
    old_elf = args.baseline_release / 'firmware/debug' / old_artifact['file']
    assert sha(old_elf) == old_artifact['sha256']
    report_path = ROOT / f'tools/verification/firmware-verification-{TAG}.json'
    current_report = json.loads(report_path.read_text())
    assert current_report['passed'] and current_report['sourcesUnchanged'] and current_report['buildExitCode'] == 0
    current_artifact = next(row for row in current_report['artifacts'] if row['suffix'] == '.elf')
    elf = env / 'build' / current_artifact['file']
    assert sha(elf) == current_artifact['sha256']
    objdump = env / 'data/packages/m5stack/tools/esp-rv32/2601/bin/riscv32-esp-elf-objdump'
    before, after = symbols(objdump, old_elf), symbols(objdump, elf)
    names = {'readerBookPages': 2052, 'jpge::s_std_lum_quant': 128,
             'jpge::s_std_croma_quant': 128, 'jpge::s_alt_quant': 128, 'fc0013_lna_gains': 192}
    rows = []
    for name, old_size in names.items():
        old, new = before[name], after[name]
        assert old['bytes'] == old_size and old['section'].startswith('.dram'), (name, old)
        if name == 'readerBookPages':
            assert new['bytes'] == 4 and new['section'].startswith('.dram'), (name, new)
            saved = old_size - new['bytes']
        else:
            assert new['bytes'] == old_size and new['section'] == '.flash.rodata', (name, new)
            saved = old_size
        rows.append({'symbol': name, 'before': old, 'after': new, 'internalStaticBytesFreed': saved})
    changed = ['Tab5EpubBookIndex.inc', 'Tab5EpubPageIndex.h', SKETCH + '.ino',
               'src/jpeg/jpge.cpp', 'src/jpeg/jpge.h', 'src/rtl_sdr/tuner_fc0013.c']
    output = {
        'version': VERSION, 'passed': True, 'physicalDeviceTested': False,
        'scope': 'Final P4 ELF object symbol sizes and section placement, authenticated before/after.',
        'baselineArchiveSha256': BASELINE_ARCHIVE_SHA256,
        'baselineElfSha256': sha(old_elf), 'currentElfSha256': sha(elf),
        'symbols': rows, 'grossInternalStaticBytesFreed': sum(row['internalStaticBytesFreed'] for row in rows),
        'sourceHashes': {f'{SKETCH}/{name}': sha(ROOT / SKETCH / name) for name in changed},
        'testSourceHashes': {Path(__file__).relative_to(ROOT).as_posix(): sha(Path(__file__))},
        'evidenceHashes': {report_path.relative_to(ROOT).as_posix(): sha(report_path)},
        'limitations': ['This gross static reduction is distinct from the net .53 DATA+BSS change, which includes the new dock.',
                       'No runtime heap, alignment overhead or device performance measurement is inferred.',
                       'EPUB PSRAM allocation and internal fallback lifecycle are separately tested by the SRAM host suite.']
    }
    (ROOT / f'tools/verification/sram-linked-{TAG}.json').write_text(json.dumps(output, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'passed': True, 'grossInternalStaticBytesFreed': output['grossInternalStaticBytesFreed']}))

if __name__ == '__main__':
    main()
