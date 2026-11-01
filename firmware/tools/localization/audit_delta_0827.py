"""Check every reviewed .27 delta against all fourteen shipped language packs."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import sys

HERE = Path(__file__).resolve().parent
RELEASE = HERE.parents[1]
SKETCH = RELEASE / 'Tab5OS_Arduino_v0.8.27_Ecriture'
sys.path.insert(0, str(RELEASE / 'tools'))
from audit_languages import signature
read = lambda path: json.loads(path.read_text(encoding='utf8'))
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()

delta = read(HERE / 'delta-0827.json')
keys = read(HERE / 'reference-final-delta-0827.json')
assert len(delta) == 14 and set(keys) == set(delta['fr-FR'])
report = {'release': '0.8.27', 'checkedAt': datetime.now(timezone.utc).isoformat(),
          'newKeys': len(keys), 'passed': True, 'sources': {}, 'packs': {}}
for name in [*read(HERE/'delta-inputs-0827.json'), 'delta-0827.json', 'reference-final-delta-0827.json', 'audit_delta_0827.py', 'apply_delta_0827.py']:
    report['sources'][name] = sha(HERE/name)
# These short international labels are intentionally identical in these languages.
technical = {'Mode', 'Format', 'Série'}
for locale, values in delta.items():
    assert set(values) == set(keys), (locale, 'incomplete delta')
    path = SKETCH / 'sdcard/System/languages' / (locale+'.json')
    pack = read(path)['translations']
    same = []
    for key, value in values.items():
        assert isinstance(value, str) and value and pack.get(key) == value, (locale, key, 'missing or different')
        assert signature(key) == signature(value), (locale, key, 'printf')
        assert key.count('\n') == value.count('\n'), (locale, key, 'newlines')
        if locale != 'fr-FR' and key == value:
            assert key in technical, (locale, key, 'untranslated')
            same.append(key)
    report['packs'][locale] = {'translatedKeys': len(keys), 'packEntries': len(pack),
                               'sharedTechnicalTerms': same, 'sha256': sha(path)}
(RELEASE / 'tools/verification/delta-languages-0827.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
print('PASS .27 delta:', len(keys), 'keys in', len(delta), 'language packs')
