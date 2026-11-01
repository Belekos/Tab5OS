#!/usr/bin/env python3
"""Compare fresh target diagnostics with the authenticated delivered .46 pass.

This is deliberately distinct from compare_warnings.py: the baseline pass is
read from delivered evidence, not re-executed. New units have no baseline count.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
from release_config import VERSION, TAG, BASELINE_VERSION, BASELINE_TAG, BASELINE_ARCHIVE_SHA256
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
digest = lambda b: hashlib.sha256(b).hexdigest()


def categories(lines):
    return dict(Counter(m.group(1) if (m := re.search(r'\[(-W[^\]]+)\]', line)) else 'unclassified' for line in lines))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment', required=True, type=Path)
    parser.add_argument('--baseline', required=True, type=Path)
    args = parser.parse_args()
    assert sha(args.baseline) == BASELINE_ARCHIVE_SHA256
    qa = args.environment/'qa'
    current_file = qa/'compiler-warning-pass.json'
    current = json.loads(current_file.read_text())
    assert current['passed'] and current['version'] == VERSION
    with zipfile.ZipFile(args.baseline) as z:
        prefix = 'Tab5OS_Ecriture_v'+BASELINE_VERSION+'/'
        member = prefix+'tools/verification/build-'+BASELINE_TAG+'/compiler-warning-pass.json'
        old_bytes = z.read(member)
        old = json.loads(old_bytes)
        assert old['passed'] and old['version'] == BASELINE_VERSION
        for name, h in old['sourceHashes'].items(): assert digest(z.read(prefix+name)) == h, name
        for name, row in old['units'].items():
            log = z.read(prefix+'tools/verification/build-'+BASELINE_TAG+'/warning-pass/'+name+'.log')
            assert digest(log) == row['logSha256'], name
    assert current['addedFlags'] == old['addedFlags'], 'Diagnostic flags differ'
    common, added = {}, {}
    for name, row in current['units'].items():
        assert row['exitCode'] == 0
        assert sha(qa/'warning-pass'/(name+'.log')) == row['logSha256']
        before = old['units'].get(name.replace(VERSION, BASELINE_VERSION))
        new = categories(row['warningLines'])
        if before:
            previous = categories(before['warningLines'])
            common[name] = {'baselineCategories': previous, 'currentCategories': new,
                            'categoryDeltas': {k: new.get(k, 0)-previous.get(k, 0) for k in sorted(set(new)|set(previous))}}
        else:
            added[name] = {'currentCategories': new, 'baselineAvailable': False}
    evidence = ROOT/'tools/verification'/('compiler-warning-current-'+TAG+'.json')
    evidence.write_bytes(current_file.read_bytes())
    report = {'version': VERSION, 'passed': True, 'physicalDeviceTested': False,
              'kind': 'comparison-to-authenticated-recorded-baseline',
              'baselineVersion': BASELINE_VERSION, 'baselineArchiveSha256': BASELINE_ARCHIVE_SHA256,
              'baselineReportMember': member, 'baselineReportSha256': digest(old_bytes),
              'baselineExecutedThisSession': False, 'currentTargetCompilationExecuted': True,
              'commonUnits': common, 'newUnits': added,
              'baselineCommonWarningCount': sum(sum(r['baselineCategories'].values()) for r in common.values()),
              'currentCommonWarningCount': sum(sum(r['currentCategories'].values()) for r in common.values()),
              'currentAllWarningCount': sum(r['warningCount'] for r in current['units'].values()),
              'sourceHashes': current['sourceHashes'],
              'testSourceHashes': {Path(__file__).relative_to(ROOT).as_posix(): sha(Path(__file__))},
              'evidenceHashes': {evidence.relative_to(ROOT).as_posix(): sha(evidence)},
              'limitations': ['Baseline diagnostics were executed and delivered with .46, not re-executed here.',
                              'Matched target SDK and warning flags; .47 has isolated M5Unified startup patch.',
                              'Counts distinguish common and new units; warnings require review, not categorical dismissal.',
                              'Diagnostic compilation cannot establish hardware safety or functional behavior.']}
    target = ROOT/'tools/verification'/('compiler-warning-recorded-comparison-'+TAG+'.json')
    target.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k: report[k] for k in ('passed','baselineCommonWarningCount','currentCommonWarningCount','currentAllWarningCount')}))


if __name__ == '__main__': main()
