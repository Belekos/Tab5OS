"""Verify downloaded Arduino archives against the pinned dependency lock."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment', type=Path, required=True)
    args = parser.parse_args()
    env = args.environment.resolve()
    lock_path = Path(__file__).resolve().with_name('dependency-downloads.json')
    lock = json.loads(lock_path.read_text())
    cli = dict(lock['cli'])
    cli['observedSha256'] = digest(env/'arduino-cli.tar.gz')
    cli['matchesExpected'] = cli['observedSha256'] == lock['cli']['observedSha256']
    if not cli['matchesExpected']:
        raise ValueError('Arduino CLI archive checksum mismatch')
    rows = []
    for source in lock['dependencies']:
        matches = list((env/'downloads').rglob(source['filename']))
        if len(matches) != 1:
            raise ValueError('Expected one downloaded archive: '+source['filename'])
        row = dict(source)
        row['observedSha256'] = digest(matches[0])
        row['archiveBytes'] = matches[0].stat().st_size
        row['matchesExpected'] = (row['observedSha256'] == source['sha256']
                                  and row['archiveBytes'] == source['archiveBytes'])
        if not row['matchesExpected']:
            raise ValueError('Dependency archive mismatch: '+source['filename'])
        rows.append(row)
    report = {'architecture':lock['architecture'], 'checkedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'lockSha256':digest(lock_path), 'verifierSha256':digest(Path(__file__)),
              'cli':cli, 'dependencies':rows, 'passed':True}
    target = env/'qa/dependency-downloads.json'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({'passed':True, 'dependencies':len(rows), 'cli':cli['version'], 'report':str(target)}))


if __name__ == '__main__':
    main()
