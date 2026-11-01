"""Check every reviewed .23 translation against the current shipped packs."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SKETCH = next(ROOT.glob('Tab5OS_Arduino_*_Ecriture'))
sys.path.insert(0, str(ROOT/'tools'))
from audit_languages import signature

sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
keys = json.loads((HERE/'reference-delta-0823.json').read_text(encoding='utf8'))
delta = json.loads((HERE/'delta-ui-0823.json').read_text(encoding='utf8'))
assert len(delta) == 14
report = {'passed': True, 'release': '0.8.23', 'checkedAt': datetime.now(timezone.utc).isoformat(),
          'newSourceKeys': len(keys), 'deltaFiles': {'delta-ui-0823.json': sha(HERE/'delta-ui-0823.json')}, 'packs': {}}
for path in sorted((SKETCH/'sdcard/System/languages').glob('*.json')):
    pack = json.loads(path.read_text(encoding='utf8'))
    code, rows = pack['id'], pack['translations']
    assert set(delta[code]) == set(keys), code
    for key in keys:
        assert rows[key] == delta[code][key] and rows[key], (code, key, 'mismatch')
        assert signature(key) == signature(rows[key]), (code, key, 'format')
        assert key.count('\n') == rows[key].count('\n'), (code, key, 'newlines')
    report['packs'][code] = {'translatedNewKeys': len(keys), 'packEntries': len(rows), 'sha256': sha(path)}
assert len(report['packs']) == 14
(ROOT/'tools/verification/delta-languages-0823.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
print('PASS .23 delta:', len(keys), 'keys in', len(report['packs']), 'packs.')
