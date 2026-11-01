#!/usr/bin/env python3
"""Apply the reviewed UI additions without replacing existing translations."""
from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from release_config import SKETCH, VERSION

def main():
    assert VERSION == '0.8.53'
    delta = json.loads(Path(__file__).with_name('ui-delta-0853.json').read_text())
    directory = ROOT / SKETCH / 'sdcard/System/languages'
    assert set(delta) == {p.stem for p in directory.glob('*.json')}
    keys = set(delta['fr-FR'])
    for language, values in delta.items():
        assert set(values) == keys
        path = directory / (language + '.json')
        data = json.loads(path.read_text())
        for key, value in values.items():
            assert key not in data['translations'] or data['translations'][key] == value, (language, key)
            data['translations'][key] = value
        path.write_text(json.dumps(data, ensure_ascii=False, separators=(',', ':')) + '\n')
    print(json.dumps({'packs': len(delta), 'reviewedAdditionsPerPack': len(keys)}))

if __name__ == '__main__':
    main()
