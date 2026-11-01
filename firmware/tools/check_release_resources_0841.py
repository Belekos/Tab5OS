"""Check the actual distributed resources and firmware recovery payload."""
import hashlib
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
sketch = root / 'Tab5OS_Arduino_v0.8.41_Ecriture'
sd = sketch / 'sdcard'
manifest = json.loads((sd / 'Factory/Tab5OS-0.8.41/manifest.json').read_text(encoding='utf8'))
assert manifest['version'] == '0.8.41'
factory = sd / 'Factory/Tab5OS-0.8.41'
assert {path.relative_to(factory).as_posix() for path in factory.rglob('*') if path.is_file()} == set(manifest['files']) | {'manifest.json'}
# The release ships English names; old user files are never moved here.
from rebuild_factory import RESOURCE_RENAMES
assert len(RESOURCE_RENAMES) == 11
assert not (set(RESOURCE_RENAMES) & set(manifest['files']))
for previous, current in RESOURCE_RENAMES.items():
    assert not (sd / previous).exists(), previous
    assert current in manifest['files'] and (sd / current).is_file(), current
assert {'Images/README.txt', 'Picture/README.txt', 'Applications/Pentest.app.elf', 'Applications/Pentest.app.elf.launch.json'} <= manifest['files'].keys()
assert (sketch / 'Tab5ImageEditor.cpp').is_file() and (sketch / 'Tab5ImageEditor.h').is_file()
assert json.loads((sd / 'Applications/Pentest.app.elf.launch.json').read_text(encoding='utf8')) == {'mode': 'none'}
for name, digest in manifest['files'].items():
    live = (sd / name).read_bytes()
    archived = (sd / 'Factory/Tab5OS-0.8.41' / name).read_bytes()
    assert live == archived, name
    assert hashlib.sha256(live).hexdigest() == digest, name
retired = {'Applications/Quake.app.elf', 'Applications/Duke3D.app.elf', 'Applications/AtelierImage.app.elf', 'Applications/AtelierImage.app.elf.launch.json'}
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
bitmap = (sketch / 'Tab5EnglishBitmap.h').read_text(encoding='utf8')
assert '"-: inward - +: around the edge"' in bitmap, 'Signed feather explanation lost its minus in the bitmap fallback'
assert not any(name.endswith(('.ttf','.elf','.zim','.t5geo')) for name in embedded)
gazetteer = {'System/maps/gazetteer/README.txt', 'System/maps/gazetteer/cities.json', 'System/maps/gazetteer/cities.t5geo'}
assert gazetteer <= manifest['files'].keys()
assert not any('gazetteer/' in name for name in embedded)
assert (sd/'System/maps/gazetteer/cities.t5geo').stat().st_size > 1000000
main = (sketch / 'Tab5OS_Arduino_v0.8.41_Ecriture.ino').read_text(encoding='utf8')
assert 'APP_VERSION[] = "0.8.41"' in main
for path in [*sketch.glob('*.cpp'),*sketch.glob('*.h'),*sketch.glob('*.ino')]:
    if path.name in ('Tab5SystemResourcesData.h','Tab5EnglishFallback.h','Tab5EnglishBitmap.h'): continue
    text = path.read_text(encoding='utf8')
    assert 'Tab5Tool::Mail' not in text and 'Tab5Tool::Browser' not in text, path.name
result = {'version':'0.8.41','factory_resources':len(manifest['files']),
          'embedded_resources':len(embedded),'python_syntax':True,'retired_apps_absent':True,
          'payload_matches_sd':True,'english_resource_renames':11,'legacy_resource_names_absent':True,'result':'PASS',
          'sourceHashes':{path.relative_to(root).as_posix():hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in [sketch/'Tab5SystemResourcesData.h',
                                       sketch/'Tab5EnglishFallback.h',
                                       sketch/'Tab5EnglishBitmap.h',
                                       sd/'Factory/Tab5OS-0.8.41/manifest.json',
                                       Path(__file__).resolve(),
                                       root/'tools/rebuild_factory.py',
                                       root/'tools/generate_system_resources.py',
                                       root/'tools/generate_locale_fallback.py']}}
destination = root / 'tools/verification/resources-0841.json'
destination.parent.mkdir(parents=True,exist_ok=True)
destination.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
print(json.dumps(result))
