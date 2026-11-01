"""Bind the current regression reports to the actual 0.8.19 source files."""
from pathlib import Path
import datetime, hashlib, json

root = Path(__file__).resolve().parents[1]
sketch = root / 'Tab5OS_Arduino_v0.8.19_Ecriture'
reports = root / 'tools/verification'

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def read(name): return json.loads((reports / name).read_text(encoding='utf-8'))
def hashes(base, mapping):
    for name, expected in mapping.items():
        path = (base / name.replace('\\', '/')).resolve()
        assert path.is_relative_to(base.resolve()) and digest(path) == expected, str(path)

names = ['firmware-install-0819.json', 'ui-controls-0819.json',
         'terminal-network-0819.json', 'network-gate-0819.json',
         'glyphs-0819.json', 'arabic-0819.json', 'ime-0819.json',
         'keyboard-shortcuts-0819.json', 'languages-0819.json',
         'shell-locale-0819.json', 'ui-locale-calls-0819.json', 'syntax-0819.json',
         'help-ui-languages-0819.json', 'pentest-buffer-0819.json']
for name in names:
    report = read(name)
    if name == 'languages-0819.json':
        assert all(not item['missingKeys'] and not item['unsupportedGlyphs']
                   and not item['formatErrors'] and not item['lineBreakErrors']
                   and not item['untranslatedLongText'] for item in report['packs'].values())
    elif name == 'network-gate-0819.json':
        assert report['status'] == 'PASS'
        assert digest(sketch / 'Tab5NetworkGate.cpp') == report['source_sha256']
    elif name == 'arabic-0819.json': assert report['pass'] is True
    else: assert report['passed'] is True, name
    if name == 'ui-controls-0819.json': hashes(root, report['sources'])
    elif name == 'pentest-buffer-0819.json':
        assert digest(sketch / 'Tab5PentestBuffer.h') == report['sourceSha256']
        assert len(report['bounds']) == 14
        assert all(row['ports'] == 1024 and row['bytes'] < report['capacity']
                   for row in report['bounds'])
    elif name == 'terminal-network-0819.json':
        assert report['sourceUnchangedDuringTests'] is True
        hashes(sketch, report['sourceSha256'])
    elif name == 'firmware-install-0819.json':
        assert digest(sketch / report['source']) == report['sha256']
        assert all(case['exitCode'] == 0 for case in report['cases'])
    elif name in ['glyphs-0819.json', 'ime-0819.json', 'keyboard-shortcuts-0819.json']:
        hashes(sketch, report['sources'])
        if 'assets' in report: hashes(sketch / 'sdcard', report['assets'])
    elif name == 'syntax-0819.json':
        for row in report['files']:
            assert row['exitCode'] == 0 and row['sourceUnchanged'] is True
            assert digest(sketch / row['name']) == row['sha256'], row['name']

# Cover all top-level implementation units, not just tests' selected files.
# The complete build has a separate manifest covering the bundled source tree.
source_hashes = {p.name: digest(p) for p in sorted(sketch.iterdir())
                 if p.is_file() and p.suffix in ['.ino', '.cpp', '.h', '.inc']}
result = dict(version='0.8.19', checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    passed=True, physicalDeviceTested=False,
    scope='Native regressions and target syntax. No physical display, Wi-Fi, USB, firmware flash or VPN tunnel test.',
    sources=source_hashes, reports={name: digest(reports / name) for name in names})
(reports / 'features-0819.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'passed': True, 'sourceUnits': len(source_hashes), 'reports': len(names)}))
