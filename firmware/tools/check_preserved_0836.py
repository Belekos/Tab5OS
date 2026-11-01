"""Verify unchanged production components against the delivered 0.8.35 ZIP.

Historical test reports retain their original version. They are not executions
of the new Explorer or main input integration, which have separate 0.8.36 tests.
"""
from pathlib import Path, PurePosixPath
import argparse
import datetime
import hashlib
import json
import zipfile

VERSION = '0.8.36'
BASELINE_VERSION = '0.8.35'
ARCHIVE_SHA256 = 'fc3c89c9e84acc6e50d29025081a1cfdb1a57437518c15ee36e77a7b6b160d47'
BASELINE_MANIFEST_SHA256 = '800460f798c0e38398e3690203af554214c2c81a66cef932c437306109118bdf'
SKETCH = 'Tab5OS_Arduino_v0.8.36_Ecriture'
BASELINE_SKETCH = 'Tab5OS_Arduino_v0.8.35_Ecriture'
CODE_SUFFIXES = {'.c', '.h', '.cpp', '.cc', '.hpp', '.inc', '.inl', '.s', '.asm'}
GENERATED = {'Tab5EnglishFallback.h', 'Tab5SystemResourcesData.h'}
CHANGED_INPUT = {'WriterUsb.h', 'WriterUsb.cpp'}
HISTORICAL_REPORTS = ('image-core-0835.json', 'image-render-0835.json',
                      'image-codec-0835.json', 'image-native-0835.json', 'map-png-0835.json')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def preserved_path(relative):
    p = PurePosixPath(relative)
    if relative == 'sdcard/System/font.ttf':
        return True
    return (p.parts[0] != 'sdcard' and p.suffix.lower() in CODE_SUFFIXES
            and not relative.startswith('tools/native-apps/image-editor/test-output/')
            and not (len(p.parts) == 1 and
                     (p.name.startswith('Tab5Explorer') or p.name in GENERATED | CHANGED_INPUT)))


def record_baseline(root, archive):
    if digest(archive) != ARCHIVE_SHA256:
        raise ValueError('The archive is not the delivered 0.8.35 baseline')
    prefix = 'Tab5OS_Ecriture_v0.8.35/'
    sketch_prefix = prefix + BASELINE_SKETCH + '/'
    with zipfile.ZipFile(archive) as z:
        if z.testzip() is not None:
            raise ValueError('Baseline archive CRC mismatch')
        files = {name[len(sketch_prefix):]:hashlib.sha256(z.read(name)).hexdigest()
                 for name in sorted(z.namelist()) if name.startswith(sketch_prefix)
                 and not name.endswith('/') and preserved_path(name[len(sketch_prefix):])}
        reports = {name:hashlib.sha256(z.read(prefix+'tools/verification/'+name)).hexdigest()
                   for name in HISTORICAL_REPORTS}
    data = {'baselineVersion':BASELINE_VERSION, 'sourceArchiveSha256':ARCHIVE_SHA256,
            'scope':'Production code, bundled native sources and system font; excludes Explorer, main sketch, WriterUsb modifier snapshots and regenerated resource/translation headers',
            'files':files, 'historicalReports':reports}
    target = root/'tools/verification/baseline-0835-preserved.json'
    target.write_text(json.dumps(data, indent=2)+'\n')


def validate(root):
    root = Path(root).resolve()
    sketch = root/SKETCH
    baseline = root/'tools/verification/baseline-0835-preserved.json'
    if digest(baseline) != BASELINE_MANIFEST_SHA256:
        raise ValueError('Baseline manifest differs from authenticated archive extraction')
    data = json.loads(baseline.read_text())
    if data['sourceArchiveSha256'] != ARCHIVE_SHA256 or data['baselineVersion'] != BASELINE_VERSION:
        raise ValueError('Unrecognized baseline release')
    expected = data['files']
    actual = {p.relative_to(sketch).as_posix():digest(p) for p in sketch.rglob('*')
              if p.is_file() and preserved_path(p.relative_to(sketch).as_posix())}
    changed = sorted(p for p in set(expected)|set(actual) if expected.get(p) != actual.get(p))
    if len(expected) != 1845 or changed:
        raise ValueError('Preserved component mismatch: '+repr(changed))
    if set(data['historicalReports']) != set(HISTORICAL_REPORTS):
        raise ValueError('Incomplete historical evidence inventory')
    for name, sha in data['historicalReports'].items():
        path = root/'tools/verification'/name
        if digest(path) != sha:
            raise ValueError('Historical report changed: '+name)
        report = json.loads(path.read_text())
        if report.get('version', report.get('release')) != BASELINE_VERSION or not (report.get('passed') is True or report.get('status') == 'passed'):
            raise ValueError('Invalid historical report: '+name)
    return {'version':VERSION, 'baselineVersion':BASELINE_VERSION, 'kind':'byte-preservation',
            'passed':True, 'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'filesChecked':len(expected), 'changed':[],
            'baselineManifest':baseline.relative_to(root).as_posix(),
            'baselineManifestSha256':digest(baseline), 'checkerSha256':digest(Path(__file__)),
            'historicalReportHashes':data['historicalReports'],
            'limitations':['No physical tablet test', 'Historical tests are not re-executed',
                           'Explorer, USB modifiers and main input integration require separate current tests',
                           'Byte preservation does not establish runtime interaction or timing on the tablet']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-archive', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    if args.baseline_archive:
        record_baseline(root, args.baseline_archive.resolve())
    report = validate(root)
    (root/'tools/verification/preserved-components-0836.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))
