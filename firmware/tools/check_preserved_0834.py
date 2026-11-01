"""Verify memory, Git, display and map files against the delivered 0.8.33 ZIP."""
from pathlib import Path
import datetime, hashlib, json


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def validate(root):
    root = Path(root).resolve()
    sketch = root/'Tab5OS_Arduino_v0.8.34_Ecriture'
    baseline = root/'tools/verification/baseline-0833-preserved.json'
    data = json.loads(baseline.read_text())
    if data['sourceArchiveSha256'] != 'c2716546724868064a0fa22808efc425d15b0ff055a92e32efb50fa965b9d8d6':
        raise ValueError('Unrecognized baseline release')
    expected = data['files']
    actual = {p.relative_to(sketch).as_posix():digest(p) for p in sketch.rglob('*') if p.is_file() and (p.relative_to(sketch).as_posix().startswith('src/tab5git/') or (p.parent == sketch and p.name.startswith(('Tab5Memory','Tab5Resource','Tab5Display','Tab5Git','Tab5Map'))))}
    changed = sorted(p for p in set(expected)|set(actual) if expected.get(p) != actual.get(p))
    if len(expected) != 582 or changed:
        raise ValueError('Preserved component mismatch: '+repr(changed))
    return {'version':'0.8.34', 'baselineVersion':'0.8.33', 'kind':'byte-preservation', 'passed':True, 'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(), 'filesChecked':len(expected), 'changed':[], 'baselineManifest':baseline.relative_to(root).as_posix(), 'baselineManifestSha256':digest(baseline), 'checkerSha256':digest(Path(__file__)), 'limitations':['No physical tablet test','Historical tests are not re-executed','Main integration and mouse input require separate current tests']}


if __name__ == '__main__':
    root = Path(__file__).resolve().parents[1]
    report = validate(root)
    (root/'tools/verification/preserved-components-0834.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))
