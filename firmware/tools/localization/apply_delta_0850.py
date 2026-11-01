"""Apply reviewed .50 labels and the single obsolete Maps help path migration."""
from pathlib import Path
import hashlib, json, zipfile
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from release_config import SKETCH, BASELINE_ARCHIVE_SHA256

def apply(baseline):
    assert hashlib.sha256(baseline.read_bytes()).hexdigest() == BASELINE_ARCHIVE_SHA256
    deltas = [json.loads(p.read_text()) for p in sorted(HERE.glob('*-delta-0850.json'))]
    folder = ROOT / SKETCH / 'sdcard/System/languages'
    prefix = 'Tab5OS_Ecriture_v0.8.49/Tab5OS_Arduino_v0.8.49_Ecriture/sdcard/System/languages/'
    with zipfile.ZipFile(baseline) as archive:
        for path in sorted(folder.glob('*.json')):
            pack = json.loads(path.read_text())
            original = json.loads(archive.read(prefix + path.name))
            rows = pack['translations']
            previous = [k for k in original['translations'] if '/Pictures/Maps' in k]
            assert len(previous) == 1, path.name
            old = previous[0]
            new = old.replace('/Pictures/Maps', '/Picture/Maps')
            value = original['translations'][old].replace('/Pictures/Maps', '/Picture/Maps')
            assert old not in rows or rows[old] == original['translations'][old]
            assert new not in rows or rows[new] == value
            rows.pop(old, None)
            rows[new] = value
            for delta in deltas:
                assert path.stem in delta, (path.name, 'missing language')
                for key, translation in delta[path.stem].items():
                    assert key not in rows or rows[key] == translation, (path.name, key, 'conflict')
                    rows[key] = translation
            path.write_text(json.dumps(pack, ensure_ascii=False, separators=(',', ':')) + '\n')
            assert path.stat().st_size <= 512 * 1024, (path.name, 'runtime size limit')
    print(json.dumps({'packs': len(list(folder.glob('*.json'))), 'deltas': len(deltas), 'obsoleteHelpKeysReplaced': 1}))

if __name__ == '__main__':
    import argparse
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline', type=Path, required=True)
    apply(p.parse_args().baseline)
