#!/usr/bin/env python3
"""Compare stabilised 0.8.49 mechanisms byte-for-byte with the supplied release."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

BASE_SHA = '0a6928c6f494f076d359d1ede44a6609a3bf9978e7970b604469787a5c0ee861'
OLD_ROOT = 'Tab5OS_Ecriture_v0.8.49/'
OLD_SKETCH = 'Tab5OS_Arduino_v0.8.49_Ecriture/'
NEW_SKETCH = 'Tab5OS_Arduino_v0.8.51_Ecriture/'

def main():
    p = argparse.ArgumentParser()
    p.add_argument('baseline', type=Path)
    p.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    args = p.parse_args()
    digest = lambda b: hashlib.sha256(b).hexdigest()
    assert digest(args.baseline.read_bytes()) == BASE_SHA, 'Wrong baseline archive'
    checks = []
    with zipfile.ZipFile(args.baseline) as archive:
        for name in archive.namelist():
            if not name.startswith(OLD_ROOT) or name.endswith('/'):
                continue
            relative = name[len(OLD_ROOT):]
            local = relative.replace(OLD_SKETCH, NEW_SKETCH, 1)
            filename = Path(relative).name
            direct = relative.startswith(OLD_SKETCH) and '/' not in relative[len(OLD_SKETCH):]
            guarded = direct and (filename.startswith(('Tab5Audio', 'Tab5Video', 'Tab5Memory', 'Tab5LvglMemory', 'Tab5Display', 'Tab5CacheSync', 'Tab5TlsMemory', 'Tab5AacMemory')) or filename in ('lv_conf.h', 'partitions.csv'))
            guarded |= relative in ('tools/build_linux/compile_linux.py',
                                    'tools/build_linux/prepare_fs_errno.py',
                                    'tools/build_linux/prepare_m5gfx_jpeg.py',
                                    'tools/build_linux/prepare_m5unified_audio.py',
                                    'tools/build_linux/prepare_dsi_cache_safe.py',
                                    'tools/build_clang/prepare_lvgl.py')
            if not guarded:
                continue
            before = archive.read(name)
            path = args.root / local
            after = path.read_bytes() if path.is_file() else b''
            checks.append({'path': local, 'baselineSha256': digest(before), 'currentSha256': digest(after), 'identical': before == after})
    result = {'version': '0.8.51', 'baselineArchiveSha256': BASE_SHA, 'physicalDeviceTested': False,
              'scope': 'Byte comparison only; this does not prove hardware behaviour.',
              'result': 'PASS' if checks and all(x['identical'] for x in checks) else 'FAIL', 'files': checks,
              'sourceHashes': {row['path']: row['currentSha256'] for row in checks},
              'testSourceHashes': {'tools/audit-0851/stability_guard.py': digest(Path(__file__).read_bytes())}}
    out = args.root / 'tools/verification/stability-preserved-0851.json'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2) + '\n')
    print(result['result'], len(checks), 'files')
    raise SystemExit(result['result'] != 'PASS')

if __name__ == '__main__':
    main()
