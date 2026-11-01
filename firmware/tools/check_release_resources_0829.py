"""Check the actual distributed resources and firmware recovery payload."""
import hashlib
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
sketch = root / 'Tab5OS_Arduino_v0.8.29_Ecriture'
sd = sketch / 'sdcard'
manifest = json.loads((sd / 'Factory/Tab5OS-0.8.29/manifest.json').read_text(encoding='utf8'))
assert manifest['version'] == '0.8.29'
assert {'Applications/Pentest.app.elf', 'Applications/Pentest.app.elf.launch.json'} <= manifest['files'].keys()
assert json.loads((sd / 'Applications/Pentest.app.elf.launch.json').read_text(encoding='utf8')) == {'mode': 'none'}
for name, digest in manifest['files'].items():
    live = (sd / name).read_bytes()
    archived = (sd / 'Factory/Tab5OS-0.8.29' / name).read_bytes()
    assert live == archived, name
    assert hashlib.sha256(live).hexdigest() == digest, name
retired = {'Applications/Quake.app.elf', 'Applications/Duke3D.app.elf'}
assert retired.isdisjoint(manifest['files'])
assert all(not (sd / name).exists() for name in retired)
assert not any((sketch / name).exists() for name in (
    'Tab5Mail.cpp', 'Tab5Browser.cpp', 'Tab5HtmlView.cpp', 'src/ReadyMail', 'src/litehtml'))
assert not any('Quake' in p.name or 'Duke3D' in p.name for p in (sd / 'Factory').rglob('*.elf'))
for path in (sd / 'MicroPython').rglob('*.py'):
    compile(path.read_text(encoding='utf8'), str(path), 'exec')
header = (sketch / 'Tab5SystemResourcesData.h').read_text(encoding='utf8')
blobs = {}
for number, literals in re.findall(r'static const char TAB5_RESOURCE_(\d+)\[\] =\n(.*?);\n', header, re.S):
    blobs[number] = ''.join(json.loads(line) for line in literals.splitlines()).encode('utf8')
embedded = []
for name, number in re.findall(r'\{"([^"]+)", TAB5_RESOURCE_(\d+), sizeof\(TAB5_RESOURCE_\d+\)-1\}', header):
    assert blobs[number] == (sd / name.lstrip('/')).read_bytes(), name
    embedded.append(name)
assert len(embedded) == len(blobs) and len(embedded) >= 30
assert not any(name.endswith(('.ttf','.elf','.zim')) for name in embedded)
main = (sketch / 'Tab5OS_Arduino_v0.8.29_Ecriture.ino').read_text(encoding='utf8')
assert 'APP_VERSION[] = "0.8.29"' in main
for path in [*sketch.glob('*.cpp'),*sketch.glob('*.h'),*sketch.glob('*.ino')]:
    if path.name in ('Tab5SystemResourcesData.h','Tab5EnglishFallback.h','Tab5EnglishBitmap.h'): continue
    text = path.read_text(encoding='utf8')
    assert 'Tab5Tool::Mail' not in text and 'Tab5Tool::Browser' not in text, path.name
result = {'version':'0.8.29','factory_resources':len(manifest['files']),
          'embedded_resources':len(embedded),'python_syntax':True,'retired_apps_absent':True,
          'payload_matches_sd':True,'result':'PASS',
          'sourceHashes':{path.relative_to(root).as_posix():hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in [sketch/'Tab5SystemResourcesData.h',
                                       sd/'Factory/Tab5OS-0.8.29/manifest.json',
                                       Path(__file__).resolve()]}}
destination = root / 'tools/verification/resources-0829.json'
destination.parent.mkdir(parents=True,exist_ok=True)
destination.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
print(json.dumps(result))
