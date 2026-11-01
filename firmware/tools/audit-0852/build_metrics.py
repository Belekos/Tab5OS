"""Record linked resource measurements without claiming physical-device results."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment', type=Path, required=True)
    parser.add_argument('--units', nargs='*', default=[])
    args = parser.parse_args()
    env = args.environment.resolve()
    proof = env / 'qa/firmware-verification-0852.json'
    data = json.loads(proof.read_text())
    assert data['version'] == '0.8.52' and data['passed']
    assert data['sourcesUnchanged'] and data['buildExitCode'] == 0
    for name, digest in data['sourceHashes'].items():
        assert sha(ROOT / name) == digest, name
    baseline_path = Path(__file__).parent / 'build-baseline.json'
    baseline = json.loads(baseline_path.read_text())
    sections = data['memoryLinkChecks']['allSections']
    old = baseline['sections']
    metrics = {}
    def add(name, before, after):
        metrics[name] = {'before': before, 'after': after, 'delta': after-before, 'unit': 'bytes'}
    add('applicationBin', baseline['firmwareBytes'], data['firmwareBytes'])
    add('otaFree', baseline['otaFreeBytes'], data['otaFreeBytes'])
    names = ('.dram0.data', '.dram1.data', '.dram0.bss', '.dram1.bss')
    add('staticDataBss', sum(old.get(n, 0) for n in names), sum(sections.get(n, 0) for n in names))
    for name in ('.iram0.text', '.tcm.text', '.tcm.data', '.rtc_noinit'):
        add(name, old[name], sections.get(name, 0))
    frames = {}
    evidence = {'tools/audit-0852/build-baseline.json': sha(baseline_path)}
    for unit in args.units:
        source = env / 'build/sketch' / (unit + '.su')
        if not source.is_file():
            raise ValueError('Missing compiler stack evidence: ' + str(source))
        target = Path(__file__).parent / ('stack-' + unit + '.su')
        shutil.copy2(source, target)
        evidence[str(target.relative_to(ROOT))] = sha(target)
        rows = []
        for line in source.read_text().splitlines():
            parts = line.split('\t')
            if len(parts) >= 3:
                rows.append({'function': parts[0], 'bytes': int(parts[1]), 'kind': parts[2]})
        frames[unit] = sorted(rows, key=lambda row: row['bytes'], reverse=True)[:12]
    local = ROOT / 'tools/verification/firmware-verification-0852.json'
    shutil.copy2(proof, local)
    evidence[str(local.relative_to(ROOT))] = sha(local)
    selected = set(args.units)
    report = {
        'version': '0.8.52', 'baselineVersion': '0.8.51', 'passed': True,
        'physicalDeviceTested': False,
        'scope': 'Actual complete ESP32-P4 linked firmware sections and selected GCC per-function frames.',
        'measurements': metrics, 'otaPartitionBytes': data['otaPartitionBytes'],
        'partitionsUnchanged': data['partitionsUnchangedFromBaseline'],
        'objectFiles': data['objectFiles'], 'sourceFilesChecked': data['sourceFilesChecked'],
        'selectedLargestFrames': frames,
        'sourceHashes': {n: h for n, h in data['sourceHashes'].items() if Path(n).name in selected or n.endswith('.ino')},
        'testSourceHashes': {str(Path(__file__).relative_to(ROOT)): sha(Path(__file__))},
        'evidenceHashes': evidence,
        'limitations': [
            'No physical Tab5 test was performed.',
            'Static sections do not measure free runtime SRAM/PSRAM, fragmentation, startup time or pointer latency.',
            'GCC frames exclude cumulative caller/callee and interrupt/task stack demand.',
            'Host UI allocation measurements exclude unmodelled OS/device services.'
        ]
    }
    (ROOT / 'tools/verification/build-resources-0852.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(metrics, indent=2))

if __name__ == '__main__':
    main()
