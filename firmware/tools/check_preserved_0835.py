"""Verify memory, Git, display and map files against the delivered 0.8.34 ZIP."""
from pathlib import Path
import datetime, hashlib, json


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def validate(root):
    root = Path(root).resolve()
    sketch = root/'Tab5OS_Arduino_v0.8.35_Ecriture'
    baseline = root/'tools/verification/baseline-0834-preserved.json'
    data = json.loads(baseline.read_text())
    if data['sourceArchiveSha256'] != '04ddb6b02b3517d9a146d5e2d83a1a5b046db765de07f7117324b7a4e791eb17':
        raise ValueError('Unrecognized baseline release')
    expected = data['files']
    actual = {p.relative_to(sketch).as_posix():digest(p) for p in sketch.rglob('*') if p.is_file() and (p.relative_to(sketch).as_posix().startswith('src/tab5git/') or (p.parent == sketch and p.name.startswith(('Tab5Memory','Tab5Resource','Tab5Display','Tab5Git','Tab5Map'))))}
    changed = sorted(p for p in set(expected)|set(actual) if expected.get(p) != actual.get(p))
    if len(expected) != 582 or changed:
        raise ValueError('Preserved component mismatch: '+repr(changed))
    return {'version':'0.8.35', 'baselineVersion':'0.8.34', 'kind':'byte-preservation', 'passed':True, 'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(), 'filesChecked':len(expected), 'changed':[], 'baselineManifest':baseline.relative_to(root).as_posix(), 'baselineManifestSha256':digest(baseline), 'checkerSha256':digest(Path(__file__)), 'limitations':['No physical tablet test','Historical tests are not re-executed','Main integration and mouse input require separate current tests','Shared PNG decoder changed and is outside this byte-preservation scope; map-png-0835.json validates it separately']}


if __name__ == '__main__':
    root = Path(__file__).resolve().parents[1]
    report = validate(root)
    (root/'tools/verification/preserved-components-0835.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))
