"""Merge and audit the reviewed .24 dictionaries without filling missing values."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import re
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
RELEASE = HERE.parents[1]
SKETCH = next(RELEASE.glob('Tab5OS_Arduino_*_Ecriture'))
sys.path.insert(0, str(RELEASE/'tools'))
from audit_languages import signature

base_keys = json.loads((HERE/'reference-delta-0824.json').read_text(encoding='utf-8'))
delta = {}
inputs = [HERE/'delta-west-0824.json', HERE/'delta-east-0824.json']
late = HERE/'delta-late-0824.json'
if late.exists():
    inputs.append(late)
help_delta = HERE/'delta-help-0824.json'
if help_delta.exists():
    inputs.append(help_delta)
for source in inputs:
    additions = json.loads(source.read_text(encoding='utf-8'))
    for locale, rows in additions.items():
        delta.setdefault(locale, {}).update(rows)
keys = list(delta['fr-FR'])
assert set(base_keys) <= set(keys)
assert len(delta) == 14
# International units, modulation names and these short shared terms are real
# identical translations. No arbitrary source/English fallback is accepted.
technical = {key for key in keys if re.fullmatch(r'(?:FM|VHF|UHF) [\d,–. -]+ MHz', key)}
technical |= {'Y min (dBFS)', 'Y max (dBFS)', 'Tuner', 'Direct I', 'Direct Q', 'Radio SDR'}
same_word = {'es-ES': {'Fin (kHz)'}}
report = {'release': '0.8.24', 'checkedAt': datetime.now(timezone.utc).isoformat(),
          'newKeys': len(keys), 'passed': True, 'sources': {}, 'packs': {}}
for source in inputs:
    report['sources'][source.name] = hashlib.sha256(source.read_bytes()).hexdigest()
for locale, additions in delta.items():
    assert set(additions) == set(keys), (locale, 'incomplete delta')
    path = SKETCH/'sdcard/System/languages'/(locale+'.json')
    rows = json.loads(path.read_text(encoding='utf-8'))['translations']
    unchanged = []
    for key, value in additions.items():
        assert isinstance(value, str) and value and rows.get(key) == value, (locale, key, 'missing/different')
        assert signature(key) == signature(value), (locale, key, 'printf')
        assert key.count('\n') == value.count('\n'), (locale, key, 'newline')
        if locale != 'fr-FR' and key == value:
            assert key in technical or key in same_word.get(locale, set()), (locale, key, 'untranslated')
            unchanged.append(key)
    report['packs'][locale] = {'translatedKeys': len(additions), 'packEntries': len(rows),
                              'sharedTechnicalTerms': unchanged,
                              'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
(HERE/'delta-0824.json').write_text(json.dumps(delta, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
(HERE/'reference-final-delta-0824.json').write_text(json.dumps(keys, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
(RELEASE/'tools/verification/delta-languages-0824.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print('PASS .24 delta:', len(keys), 'keys in', len(delta), 'language packs')
