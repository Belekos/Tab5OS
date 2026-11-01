"""Record and preserve the exact delivered .25 evidence before .26 verification.

Only generic build/publication outputs are copied into the new history folder;
versioned reports and older history folders remain at their original paths.
The archive itself is mandatory and checked before any output is written.
"""
from pathlib import Path
import hashlib
import json
import re
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE = ROOT.parent / 'Tab5OS_0.8.25_corrections.zip'
SHA256 = '0575a946cf9e9f98e3cd0cdc6870b4221fb028d028415aabeb95672dd9f2527d'
REPORTS = ROOT / 'tools/verification'
PREFIX = 'Tab5OS_Ecriture_v0.8.25/'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    assert ARCHIVE.stat().st_size == 69251012, 'Unexpected baseline ZIP size'
    assert sha(ARCHIVE.read_bytes()) == SHA256, 'Unexpected baseline ZIP hash'
    with zipfile.ZipFile(ARCHIVE) as archive:
        assert len(archive.infolist()) == 3218 and archive.testzip() is None
        reports, files, preserved = {}, {}, {}
        for item in archive.infolist():
            if not item.filename.startswith(PREFIX + 'tools/verification/') or item.is_dir():
                continue
            name = item.filename.removeprefix(PREFIX + 'tools/verification/')
            assert '..' not in Path(name).parts
            data = archive.read(item)
            # Root reports without a version suffix are outputs of the next
            # full build. Keep their exact .25 bytes under an immutable path.
            generic = '/' not in name and not re.search(r'-08\d{2}(?:\.|$)', name)
            target_name = 'history-0825/evidence/' + name if generic else name
            target = REPORTS / target_name
            if generic:
                preserved[target] = data
            else:
                assert target.is_file() and sha(target.read_bytes()) == sha(data), 'Changed historical evidence: ' + name
            files[name] = {'sha256': sha(data), 'bytes': len(data), 'preservedAs': target_name}
            if '/' not in name and name.endswith('.json'):
                reports[name] = sha(data)
        contracts = {}
        for name in ('verify_features_0825.py', 'feature_contracts_0825.py'):
            data = archive.read(PREFIX + 'tools/' + name)
            assert sha((ROOT / 'tools' / name).read_bytes()) == sha(data), 'Changed .25 contract: ' + name
            contracts[name] = sha(data)
        index = {'baselineVersion': '0.8.25', 'baselineArchiveSha256': SHA256,
                 'baselineArchiveBytes': 69251012, 'baselineArchiveFiles': 3218,
                 'verifierSha256': contracts['verify_features_0825.py'],
                 'contractSha256': contracts, 'reports': reports, 'files': files}
        for target, data in preserved.items():
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists():
                assert target.read_bytes() == data, 'Refusing to change archived baseline'
            else:
                target.write_bytes(data)
        target = REPORTS / 'history-0825/index.json'
        data = (json.dumps(index, ensure_ascii=False, indent=2) + '\n').encode()
        if target.exists():
            assert target.read_bytes() == data, 'Refusing to change historical index'
        else:
            target.write_bytes(data)
        print(json.dumps({'preservedFiles': len(files), 'reports': len(reports),
                          'archivedGenericFiles': len(preserved), 'indexSha256': sha(data)}))


if __name__ == '__main__':
    main()
